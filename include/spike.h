
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

#ifndef SPIKE_H_
#define SPIKE_H_

#ifndef PTHREAD_HEADER_INCLUDED_
#define PTHREAD_HEADER_INCLUDED_
#include <pthread.h>
#endif // PTHREAD_HEADER_INCLUDED_

typedef struct {
	int pipe_read;
	int client_fd;
	pthread_mutex_t* stats_mutex;
	char** stats;
} watchdog_thread_args;

typedef struct {
	pthread_t th_id;
	int pipe_write;
} watchdog_thread;

FILE* get_stat_filep (char* pid);

char* get_proc_string (FILE* fp);

int run_watchdog (int pids[], int num_pids, char* port, char* email_address, char* email_server);

#endif // SPIKE_H_

