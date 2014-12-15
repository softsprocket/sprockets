
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
	pid_t pid;
	char pcomm[1024];
	char state;
	pid_t ppid;
	pid_t pgrp;
	pid_t session;
	int tty_nr;
	pid_t tpgid;
	unsigned int flags;
	long unsigned int minflt;
	long unsigned int cminflt;
	long unsigned int majflt;
	long unsigned int cmajflt;
	long unsigned int utime;
	long unsigned int stime;
	long int cutime;
	long int cstime;
	long int priority;
	long int nice;
	long int num_threads;
	long int itrealvalue;
	long long int starttime; 
	long unsigned int vsize;
	long int rss;
	long unsigned int rsslim;
	long unsigned int startcode;
	long unsigned int endcode;
	long unsigned int startstack;
	long unsigned int kstkesp;
	long unsigned int kstkeip;
	long unsigned int signal;
	long unsigned int blocked;
	long unsigned int sigignore;
	long unsigned int sigcatch;
	long unsigned int wchan;
	long unsigned int nswap;
	long unsigned int cnswap;
	int exit_signal;
	int processor;
	unsigned int rt_priority;
	unsigned int policy;
	long long unsigned int delayacct_blkio_ticks;
	long unsigned guest_time;
	long int cguest_time;
	long unsigned int start_data;
	long unsigned int end_data;
	long unsigned int start_brk;
	long unsigned int arg_start;
	long unsigned int arg_end;
	long unsigned int env_start;
	long unsigned int env_end;
	int exit_code;
} watchdog_procstat;

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

watchdog_procstat* get_proc_stat (FILE* fp);

int get_proc_str (watchdog_procstat* ps, char** return_str, size_t max_sz);

int run_watchdog (int pids[], int num_pids, char* port, char* email_address, char* email_server);

#endif // SPIKE_H_

