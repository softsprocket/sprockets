
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
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <syslog.h>
#include <dlfcn.h>
#include <string.h>
#include <getopt.h>

#include "sprocket.h"
#include "spike.h"

int* start_servers (hash_table* document, int* num_servers);
int fork_sprocket (hash_table* server);

void usage (char* ex) {
	fprintf (stderr, "usage:%s [-w -e <email_address>] serc_filename", ex);
	exit (EXIT_FAILURE);
}

int main (int argc, char* argv[]) {
	int watchdog = 0;
	char* email_address = NULL;
	int opt =0;
	while ((opt = getopt (argc, argv, "hwe:")) != -1) {
		switch (opt) {
			case 'h':
				usage (argv[0]);
			case 'w':
				watchdog = 1;
				break;
			case 'e':
				email_address = optarg;
				break;
			default:
				fprintf (stderr, "Unknown option '%d'\n", opt);
				break;
		}
	}

	if (optind >= argc) {
		usage (argv[0]);
	}

	char* fname = argv[optind];

	size_t buffer_size = 0;
	char* read_buf = serc_read_file (fname, &buffer_size);

	if (read_buf == NULL) {
		PMSG ("serc_read_file");
		exit (EXIT_FAILURE);
	}

	hash_table* document = serc_parse_buffer (read_buf, buffer_size);
	if (document == NULL) {
		PMSG ("serc_parse_buffer");
		free (read_buf);
		exit (EXIT_FAILURE);
	}

	free (read_buf);

	int* pids = NULL;
	int num_servers = 0;
	if ((pids = start_servers (document, &num_servers)) == NULL) {
		PMSG ("start_servers failed");
		delete_document (document);
		exit (EXIT_FAILURE);
	}

	if (watchdog) {
		run_watchdog (pids, num_servers, email_address);
	}

	free (pids);
	delete_document (document);

	return EXIT_SUCCESS;
}

int* start_servers (hash_table* document, int* num_servers) {
	storage_unit* su = hash_table_get (document, "servers");
	if (su == NULL) {
		PMSG ("unable to retrieve \"servers\" config element");
		return NULL;
	}

	auto_array* servers = SU_TUPLE_TO_ARRAY (su);
	*num_servers = servers->count;
	int* pids = malloc (sizeof *pids * *num_servers);
	if (pids == NULL) {
		PERR ("malloc");
		return NULL;
	}

	for (int i = 0; i < *num_servers; ++i) {
		hash_table* server = auto_array_get (servers, i);
		int pid = fork_sprocket (server);
		pids[i] = pid;
	}


	return pids;
}

int fork_sprocket (hash_table* server) {
	int fd[2];
	
	int OK = 0;
	int ERR = 1;

	pipe (fd);

	pid_t pid = fork();
	if (pid < 0) {
		PERR ("fork");
		exit (EXIT_FAILURE);
	}
	
	storage_unit* su_id = hash_table_get (server, "id");
	char* id = SU_TUPLE_TO_STRING (su_id);

	if (pid == 0) {
		close(fd[0]);
			
		storage_unit* su_so = hash_table_get (server, "shared_object");
		char* shared_object = SU_TUPLE_TO_STRING (su_so);

		umask(0);

		openlog (id, LOG_CONS, LOG_DAEMON);

		pid_t sid = setsid();
		if (sid < 0) {
			syslog (LOG_CRIT, "setsid failed for %s\n", id);
			write (fd[1], &ERR, sizeof ERR);
			exit (EXIT_FAILURE);
		}

		void* handle = dlopen(shared_object, RTLD_LAZY);
		if (handle == NULL) {
			fprintf (stderr, "%s\n", dlerror ());
			syslog (LOG_CRIT, "dlopen failed for %s, %s\n", id, dlerror ());
			write (fd[1], &ERR, sizeof ERR);
			exit (EXIT_FAILURE);
		}

		if ((chdir("/")) < 0) {
			syslog (LOG_CRIT, "chdir failed for %s\n", id);
			write (fd[1], &ERR, sizeof ERR);
			exit (EXIT_FAILURE);
		}

		int (*main_exec)(hash_table*) = dlsym (handle, "exec"); 
		
		if (main_exec == NULL) {
			fprintf (stderr, "%s\n", dlerror ());
			syslog (LOG_CRIT, "dlsym failed for %s, %s\n", id, dlerror ());
			write (fd[1], &ERR, sizeof ERR);
			exit (EXIT_FAILURE);
		}

		hash_table* config = NULL;
		storage_unit* su_config = hash_table_get (server, "config");
		if (su_config != NULL) {
			hash_table* tmp = create_map (id, su_config);
			auto_string* aa = serc_document_to_auto_string (tmp);
			if (aa != NULL) {
				size_t sz;
				char* buf = parseable_buffer_from_string (aa->buf, &sz);
				if (buf != NULL) {
		       			config = serc_parse_buffer (buf, sz);
					free (buf);
				} else {
					PMSG ("parseable_buffer_from_string returned NULL");
				}
			} else {
				PMSG ("serc_document_to_auto_string returned NULL");
			}		
		} else {
			syslog (LOG_NOTICE, "%s starting without config", id);
		}

		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);

		write (fd[1], &OK, sizeof OK);
		close (fd[1]);
			
		main_exec (config);
		
		delete_document (config);
		dlclose (handle);
		syslog (LOG_NOTICE, "sprocket %s exiting\n", id);	
		exit (EXIT_SUCCESS);
	} else {
		close (fd[1]);

		int resp = 0;
		read (fd[0], &resp, sizeof resp);

		if (resp == OK) {
			printf ("%s started successfully on %d\n", id, pid);
		} else {
			printf ("%s start failed\n", id);
			pid = 0;
		}
		close (fd[0]);
	}


	return pid;
}

