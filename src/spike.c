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

#include "sprocket.h"

#define MAX_EPOLL_EVENTS 10

int run_watchdog (int pids[], int num_pids, char* email_address) {

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

		int sigs[] = { SIGINT, SIGQUIT, SIGHUP };
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

		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);

		syslog (LOG_NOTICE, "Sprockets watchdog successfully started\n");

		for (int i = 0; i < num_pids; ++i) {
			syslog (LOG_NOTICE, "Watching pid %d\n", pids[i]);
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

			if (num_fd == 0) { // no fds  timeout occurred

			} else { 
				int signal_caught = 0;
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
								syslog (LOG_NOTICE, "Caught SIGINT - exiting\n");
								exit (EXIT_SUCCESS);
							case SIGQUIT:
								syslog (LOG_NOTICE, "Caught SIGQUIT - exiting\n");
								exit (EXIT_SUCCESS);
							case SIGHUP:
								syslog (LOG_NOTICE, "Caught SIGHUP\n");
								signal_caught = 1;
								break;
							default:
								syslog (LOG_NOTICE, "Caught unknown signal\n");
								break;
						}
	       				}
				}

				if (signal_caught) {
					syslog (LOG_NOTICE, "Caught unknown signal\n");
				}	
			}
		}

	}

	return 0;
}


