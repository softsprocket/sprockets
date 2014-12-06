
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

#include <sys/socket.h>
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

#include "sprocket.h"

int start_servers (hash_table* document);
int fork_sprocket (hash_table* server);

int main (int argc, char* argv[]) {
	if (argc < 2) { 
		fprintf (stderr, "usage:%s serc_filename", argv[0]);
		exit (EXIT_FAILURE);
	}

	char* fname = argv[1];

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

	if (start_servers (document) == 0) {
		PMSG ("start_servers failed");
		delete_document (document);
		exit (EXIT_FAILURE);
	}

	delete_document (document);

	return EXIT_SUCCESS;
}

int start_servers (hash_table* document) {
	storage_unit* su = hash_table_get (document, "servers");
	if (su == NULL) {
		PMSG ("unable to retrieve \"servers\" config element");
		return 0;
	}

	auto_array* servers = SU_TUPLE_TO_ARRAY (su);

	for (int i = 0; i < servers->count; ++i) {
		hash_table* server = auto_array_get (servers, i);
		fork_sprocket (server);
	}

	return 1;
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
		}
		close (fd[0]);
	}


	return pid;
}

