/** @file */
/**
*    **Sprockets** - server execution software components
*    ====================================================
*    Copyright (C) 2014 Gregory Ralph Martin
*    info at softsprocket dot com
*
*    This library is free software; you can redistribute it and/or
*    modify it under the terms of the GNU Lesser General Public
*    License as published by the Free Software Foundation; either
*    version 2.1 of the License, or (at your option) any later version.
*
*    This library is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*    Lesser General Public License for more details.
*
*    You should have received a copy of the GNU Lesser General Public
*    License along with this library; if not, write to the Free Software
*    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
*    USA
*/

#include <softsprocket/debug_utils.h>
#include <softsprocket/container.h>
#include <softsprocket/serc.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#include "sprocket.h"
#include "spike.h"

#define MAX_EPOLL_EVENTS 10

char* email_alert_msg_format = 
	"Subject: Sprockets alert\r\n"
	"From: %s\r\n"
	"To: %s\r\n\r\n"
	"Process with pid %s has halted\r\n"
	"\r\n.\r\n";

void* client_thread (void* args) {
	watchdog_thread_args* dog_args = args;

	signal (SIGPIPE, SIG_IGN);

	int keep_running = 1;
	while (keep_running) {
		char msg[3];
		int rv = 0;
		int  nr = 2;
		char* pmsg = msg;
		msg[2] = '\0';

		while ((rv = read (dog_args->pipe_read, pmsg, nr)) > 0) {
			nr -= rv;
			if (nr != 0) {
				pmsg += rv;
			} else {
				if (strcmp (msg, "OK") == 0) {
					pthread_mutex_lock (dog_args->stats_mutex);
					char* err_msg = "No stats available\n";
					char* msg;
					if (*dog_args->stats == NULL) {
						msg = err_msg;
					} else {
						msg = *dog_args->stats;
					}

					int wv = 0;
					if ((wv = write (dog_args->client_fd, msg, strlen (msg))) < 0) {
						pthread_mutex_unlock (dog_args->stats_mutex);
						syslog (LOG_CRIT, "client thread exiting %s", strerror (errno));
					 	keep_running = 0;
					  	break;	
					}

    					pthread_mutex_unlock (dog_args->stats_mutex);
					if (wv == 0) {
						syslog (LOG_CRIT, "client thread EOF");
					 	keep_running = 0;
					  	break;	
					}

				} else if (strcmp (msg, "EX") == 0) {
					keep_running = 0;
				} else {
					syslog (LOG_CRIT, "client thread recieved unknow msg %s\n", msg);		
				}

			}		
		}
		
		if (rv < 0) {
			syslog (LOG_CRIT, "watchdog client thread: %s", strerror (errno)); 
			break;
		}
	}

	syslog (LOG_NOTICE, "client thread cleaning up");
	close (dog_args->pipe_read);
	close (dog_args->client_fd);
	free (args);
	syslog (LOG_NOTICE, "client thread exiting");

	return NULL;
}

void close_file (void* f) {
	fclose (f);
}

int run_watchdog (int pids[], int num_pids, char* port, char* email_address, char* mail_server) {
	signal (SIGPIPE, SIG_IGN);

	pid_t pid = fork();
	if (pid < 0) {
		PERR ("fork");
		exit (EXIT_FAILURE);
	}

	if (pid == 0) {

		umask(0);

		openlog ("SPROCKETS_WATCHDOG", LOG_CONS, LOG_DAEMON);

		pid_t sid = setsid();
		if (sid < 0) {
			PMSG ("Sprockets watchdog failed to setsid. Exiting now.\n");
			exit (EXIT_FAILURE);
		}

		if ((chdir("/")) < 0) {
			PMSG ("Sprockets watchdog failed to chdir. Exiting now.\n");
			exit (EXIT_FAILURE);
		}

		int sigs[] = { SIGINT, SIGQUIT, SIGHUP, SIGCHLD };
		int sfd = setup_sighandlers (sigs, 3);
		if (sfd == 0) {
			PMSG ("Sprockets watchdog failed to set sighandlers. Exiting now.\n");
			exit (EXIT_FAILURE);
		}

		int epfd = init_epoll (sfd);
		if (epfd == 0) {
			PMSG ("Sprockets watchdog failed to initialize epoll. Exiting now.\n");
			exit (EXIT_FAILURE);
		}

		int server_fd = NULL;
		if (port != NULL) {
			server_fd = sprocket_tcp_server (port, NULL);
			if (server_fd == 0) {
				PMSG ("Couldn't start watchdog server\n");
			       	exit (EXIT_FAILURE);	
			}

			if (add_fd_to_epoll (epfd, server_fd) == 0) {
				PERR ("Couldn't add watchdog server to epoll\n");
			       	exit (EXIT_FAILURE);	
			}
		}

		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);

		syslog (LOG_NOTICE, "Sprockets watchdog successfully started\n");



				
		int paused_for_signal = 0;
		char* stats = NULL;
		auto_string* stat_buffer = NULL;
		pthread_mutex_t stats_mutex;
		pthread_mutex_init(&stats_mutex, NULL);

		auto_array* thread_array = auto_array_create (10);
		if (thread_array == NULL) {
			syslog (LOG_CRIT, "Unable to create thread array - out of memory");
			exit (EXIT_FAILURE);
		}


		while (1) {
			struct epoll_event events[MAX_EPOLL_EVENTS];
			int num_fd = epoll_wait(epfd, events, MAX_EPOLL_EVENTS, 500);

			if (num_fd == -1) {
				if (errno == EINTR) {
					syslog (LOG_NOTICE, "epoll_wait interrupted. Continuing\n");
					continue;
				}

				syslog (LOG_CRIT, "epoll_wait error: %s\n", strerror (errno)); 
				exit (EXIT_FAILURE);
			}

			if (num_fd != 0) { // no fds  timeout occurred
				for (int i = 0; i < num_fd; ++i) {
					if (events[i].data.fd == sfd) { // caught signal
						struct signalfd_siginfo fdsi;

 						int s = read (sfd, &fdsi, sizeof fdsi);
               					if (s != sizeof fdsi) {
							syslog (LOG_CRIT, "Read signal error: %s\n", strerror (errno));
							continue;
						}

						switch (fdsi.ssi_signo) {
							case SIGINT:
								syslog (LOG_NOTICE, "Caught SIGINT - pausing\n");
								paused_for_signal = 1;
								break;	
							case SIGQUIT:
								syslog (LOG_NOTICE, "Caught SIGQUIT - exiting\n");
								for (int ii = 0; ii < num_pids; ++ii) {
									if (pids[ii] != 0) { 
										kill (pids[ii], SIGTERM);
									}
								}

								for (int ii = 0; ii < thread_array->count; ++ii) {
									watchdog_thread* wt = auto_array_get (thread_array, ii);
									char ex[2] = "EX";
									write (wt->pipe_write, ex, 2);

								       	auto_array_delete (thread_array, free);	
								}

								exit (EXIT_SUCCESS);
							case SIGHUP:
								syslog (LOG_NOTICE, "Caught SIGHUP\n");
								paused_for_signal = 0;
								break;
							case SIGCHLD:
								syslog (LOG_NOTICE, "Caught SIGCHLD\n");
								break;
							default:
								syslog (LOG_NOTICE, "Caught unknown signal\n");
								break;
						}
	       				} else if (events[i].data.fd == server_fd) {
						struct sockaddr_in addr;
						socklen_t addr_sz = 0;
						int client_fd = accept (server_fd, (struct sockaddr*) &addr, &addr_sz);
						
						watchdog_thread_args* thread_args = malloc (sizeof thread_args);
						if (thread_args == NULL) {
							syslog (LOG_CRIT, "Malloc returned NULL: %s", strerror (errno));
							exit (EXIT_FAILURE);
						}

						watchdog_thread* dog_thread = malloc (sizeof dog_thread);
						if (dog_thread == NULL) {
							syslog (LOG_CRIT, "Malloc returned NULL: %s", strerror (errno));
							exit (EXIT_FAILURE);
						}

						int pipefd[2];
						if (pipe (pipefd) < 0) {
							syslog (LOG_CRIT, "watchdog pipe error: %s", strerror (errno));
							continue;
						}
						
						dog_thread->pipe_write = pipefd[1];

						thread_args->pipe_read = pipefd[0];
						thread_args->client_fd = client_fd;
						thread_args->stats = &stats;
						thread_args->stats_mutex = &stats_mutex;

						pthread_attr_t attr;
						int trv = 0;
						if ((trv = pthread_attr_init (&attr)) != 0) {
							syslog (LOG_CRIT, "pthread_attr_init: %s\n", strerror (trv));
						       	continue;	
						}

						if ((trv = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) != 0) {
							syslog (LOG_CRIT, "pthread_attr_setdetachstate: %s\n", strerror (trv));
						       	continue;	
						}

						if ((trv = pthread_create(&dog_thread->th_id, &attr, client_thread, thread_args)) != 0) {
							syslog (LOG_CRIT, "pthread_create: %s\n", strerror (trv));
						       	continue;	
						}

						auto_array_add (thread_array, dog_thread);

						pthread_attr_destroy(&attr);
					}
				}

			}

			if (paused_for_signal) {
				syslog (LOG_NOTICE, "Paused for signal\n");
			} else {
				auto_array* stat_files = auto_array_create (num_pids);
				for (int i = 0; i < num_pids; ++i) {
					if (pids[i] == 0) {
						continue;
					}

					char pid[32];
					sprintf (pid, "%d", pids[i]);
					
					FILE* p = get_stat_filep (pid);
					if (p == NULL) {
						syslog (LOG_CRIT, "Process with pid %s has halted", pid);
						if (email_address != NULL && mail_server != NULL) {
							char msg[2048];
							snprintf (msg, 2048, email_alert_msg_format, email_address, email_address, pid);
							send_email (mail_server, email_address, email_address, msg);
									
							pids[i] = 0; 
						}

						continue;
					}

					auto_array_add (stat_files, p);
				}

				pthread_mutex_lock(&stats_mutex);

				if (stat_buffer != NULL) {
					auto_string_delete (stat_buffer);
				}
				
				stat_buffer = auto_string_create (1024);
				if (stat_buffer == NULL) {
					syslog (LOG_CRIT, "Unable to create stat buffer - out of memory");
					exit (EXIT_FAILURE);
				}

				for (int i = 0; i < stat_files->count; ++i) {
					FILE* f = auto_array_get (stat_files, i);
					
					watchdog_procstat* ps = get_proc_stat (f);
					if (ps == NULL) {
						syslog (LOG_CRIT, "Process with pid %d has halted", pid);
						continue;
					}
					

					size_t sz = 1024;
					char* tmp = NULL;

					while (1) {
						tmp = malloc (sz);
						if (tmp == NULL) {
							syslog (LOG_CRIT, "Malloc: %s", strerror (errno));
							exit (EXIT_FAILURE);
						}

						int rv = get_proc_str (ps, &tmp, sz);
						if (rv >= sz) {
							free (tmp);
							tmp = NULL;
							sz *= 2;
						} if (rv < 0) {
							syslog (LOG_CRIT, "get_proc_str: %s", strerror (errno));
							free (tmp);
							tmp = NULL;
							break;
						} else {
							break;
						}
					}
					free (ps);
					if (tmp != NULL) {
						auto_string_append (stat_buffer, tmp);
						free (tmp);	
					}
				}

				stats = stat_buffer->buf;

				pthread_mutex_unlock (&stats_mutex);

				for (int i = 0; i < thread_array->count; ++i) {
					watchdog_thread* wt = auto_array_get (thread_array, i);
					char ok[2] = "OK";
					if (write (wt->pipe_write, ok, 2) <= 0) {
						syslog (LOG_CRIT, "watchdog write client thread: %s", strerror (errno));
						auto_array_remove (thread_array, i);
						close (wt->pipe_write);
						free (wt);
					}
				}
				
				auto_array_delete (stat_files, close_file);
			}	
		}

	}

	return pid;
}



FILE* get_stat_filep (char* pid) {
	size_t sz = strlen (pid) + 12; // 11 char is format string

	char* path = malloc (sz);
	if (path == NULL) {
		syslog (LOG_CRIT, "malloc: %s\n", strerror (errno));
		exit (EXIT_FAILURE);
	}

	
	snprintf (path, sz, "/proc/%s/stat", pid);
	
	FILE* fp = fopen (path, "r");
	if (fp == NULL) {
		syslog (LOG_CRIT, "fopen on stat file - %s : %s\n", path, strerror (errno));
		free (path);

		return NULL;
	}

	free (path);

	return fp;
}


watchdog_procstat* get_proc_stat (FILE* fp) {
	watchdog_procstat* ps = malloc (sizeof ps);
       	if (ps == NULL) {
		syslog (LOG_CRIT, "malloc: %s\n", strerror (errno));
		exit (EXIT_FAILURE);
	}

	rewind (fp);

	if (fscanf (fp, "%d %s %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu %ld %ld %ld "
				"%ld %ld %ld %llu %lu %ld %lu %lu %lu %lu %lu %lu %lu %lu "
				"%lu %lu %lu %lu %lu %d %d %u %u %llu %lu %ld %lu %lu %lu %lu %lu %lu %lu %d", 
				&ps->pid, ps->pcomm, &ps->state, &ps->ppid, &ps->pgrp, &ps->session, &ps->tty_nr, 
				&ps->tpgid, &ps->flags, &ps->minflt, &ps->cminflt, &ps->majflt, &ps->cmajflt, &ps->utime, &ps->stime,
				&ps->cutime, &ps->cstime, &ps->priority, &ps->nice, &ps->num_threads, &ps->itrealvalue, &ps->starttime, 
				&ps->vsize, &ps->rss, &ps->rsslim, &ps->startcode, &ps->endcode, &ps->startstack, 
				&ps->kstkesp, &ps->kstkeip, &ps->signal, &ps->blocked, &ps->sigignore, &ps->sigcatch, &ps->wchan, 
				&ps->nswap, &ps->cnswap, &ps->exit_signal, &ps->processor, &ps->rt_priority,
				&ps->policy, &ps->delayacct_blkio_ticks, &ps->guest_time, &ps->cguest_time, &ps->start_data, 
				&ps->end_data, &ps->start_brk, &ps->arg_start, &ps->arg_end, &ps->env_start, &ps->env_end, &ps->exit_code) < 0) {
	
		syslog (LOG_CRIT, "fscanf on pid status error: %s", strerror (errno));
		return NULL;

	}


	return ps;
}
/*
 * pid, command, state, minor faults, child minor faults, major faults, child major faults, user time, kernel time
 * child user time, child kernel time. priority, nice, number of threads, vmem size, resident set size, rss limit,
 * bottom stack, stack pointer, instruction pointer, processor, rt priority, data start address, data end address,
 * program start address, program end address
 *
 */
int get_proc_str (watchdog_procstat* ps, char** return_str, size_t max_sz) {
	return snprintf (*return_str, max_sz, "%d %s %c %lu %lu %lu %lu %lu %lu %ld %ld %ld %ld %ld %ld %lu %lu %lu %lu "
			"%lu %d %u %lu %lu %lu %lu %d\n", 
			ps->pid, ps->pcomm, ps->state, ps->minflt, ps->cminflt, ps->majflt, ps->cmajflt, ps->utime, ps->stime,
			ps->cutime, ps->cstime, ps->priority, ps->nice, ps->num_threads, ps->vsize, ps->rss, ps->rsslim, 
			ps->startstack, ps->kstkesp, ps->kstkeip, ps->processor, ps->rt_priority, ps->start_data,
		        ps->end_data, ps->env_start, ps->env_end, ps->exit_code);	
}

