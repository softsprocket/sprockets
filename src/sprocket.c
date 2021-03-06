
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
#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <string.h>

#include "sprocket.h"

#define LN() syslog (LOG_NOTICE, "%s %d\n", __FILE__, __LINE__);

int sprocket_tcp_server (char* port, char* host) {
	struct addrinfo hints;
	memset (&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM;

	if (host == NULL) {
		hints.ai_flags = AI_PASSIVE;   /*  For wildcard IP address */
	} else {
		hints.ai_flags = 0;
	}

	hints.ai_protocol = 0;
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	struct addrinfo *result;
	int s = getaddrinfo (host, port, &hints, &result);
	if (s != 0) {
		syslog (LOG_CRIT, "getaddrinfo: %s\n", gai_strerror(s));
		return 0;
	}

	
	struct addrinfo *rp;
	int fd = 0;
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		fd = socket (rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (fd == -1) {
			continue;
		}

		if (bind (fd, rp->ai_addr, rp->ai_addrlen) == 0) {
			break;                  
		} else {
			syslog (LOG_INFO, "bind: %s", strerror (errno));
		}
		
		close(fd);
	}

	if (rp == NULL) {              
		syslog (LOG_CRIT, "server: unable to bind socket\n");	
		freeaddrinfo (result);

		return 0;
	}

	freeaddrinfo (result);

	int optval = 1;
    	setsockopt (fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof optval);

	int flags = fcntl (fd, F_GETFL, 0);

	if (flags == -1) {
		syslog (LOG_CRIT, "fcntl: %s", strerror (errno));
		return 0;
	}

	flags |= O_NONBLOCK;

	if (fcntl (fd, F_SETFL, flags) == -1) {
		syslog (LOG_CRIT, "fcntl: %s", strerror (errno));
		return 0;
	}

	if (listen(fd, 5) == -1) {
		syslog (LOG_CRIT, "listen: %s", strerror (errno));
		return 0;
	}

	return fd;
}

int sprocket_tcp_client (char* port, char* host) {
	struct addrinfo hints;
	memset (&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	hints.ai_flags = 0;

	struct addrinfo *result;
	int s = getaddrinfo (host, port, &hints, &result);
	if (s != 0) {
		syslog (LOG_CRIT, "getaddrinfo: %s\n", gai_strerror(s));
		return 0;
	}

	
	struct addrinfo *rp;
	int fd = 0;
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		fd = socket (rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (fd == -1) {
			continue;
		}

		if (connect (fd, rp->ai_addr, rp->ai_addrlen) != -1) {
			break;                  
		}

		close(fd);
	}

	if (rp == NULL) {              
		syslog (LOG_CRIT, "client: unable to connect socket\n");	
		freeaddrinfo (result);

		return 0;
	}

	freeaddrinfo (result);

	return fd;
}


int init_epoll (int fd) {
	int epfd = epoll_create (1);

	if (epfd == -1) {
		syslog (LOG_CRIT, "epoll_create: %s", strerror (errno));
		return 0;
	}

	
	return add_fd_to_epoll (epfd, fd);
}

int add_fd_to_epoll (int epfd, int fd) {
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = fd;

	if (epoll_ctl (epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
		syslog (LOG_CRIT, "epoll_ctrl: %s", strerror (errno));
		return 0;
	}

	return epfd;
}

int setup_sighandlers (int sigs[], int num_sigs) {
	sigset_t mask;

	sigemptyset(&mask);

	for (int i = 0; i < num_sigs; ++i) {
		sigaddset(&mask, sigs[i]);
	}

	if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
		PERR ("sigprocmask");
		return 0;	
	}

	int sfd = signalfd(-1, &mask, 0);

	if (sfd == -1) {
		PERR ("signalfd");
		return 0;
	}

	return sfd;
}

int smtp_get_reply (int sfd) {
	int more = 1;
	char line[512];
	char* pline = line;
	int count = 0;
	do {
		int rv = read (sfd, pline, 1);
		if (rv == 1) {
			count++;
		
			if (count == 511 || *pline == '\n') {
				more = 0;
				*(pline + 1) = '\0';	
			}

			++pline;
		} else if (rv == 0) {
			syslog (LOG_CRIT, "SMTP EOF");
			more = 0;
			*pline = '\0';
		} else if (rv == -1) {
			if (errno == EINTR) {
				continue;
			}
			syslog (LOG_CRIT, "SMTP read: %s", strerror (errno));
			return 0;
		}
	} while (more);

	syslog (LOG_NOTICE, "SMTP reply: %s", line);

	return atoi (line);
}

int smtp_send (int sfd, char* line) {
	syslog (LOG_NOTICE, "Sending %s", line);
	int nr = strlen (line);
	char* pline = line;
	do {
		int rv = write (sfd, pline, nr);
		if (rv == 0) {
			syslog (LOG_CRIT, "SMTP unknown write error: %s", line);
			return 0;
		} else if (rv == -1) {
			if (errno == EINTR) {
				syslog (LOG_NOTICE, "SMTP write interrupted");
				continue;
			}
			
			syslog (LOG_CRIT, "SMTP write error: %s", strerror (errno));
			return 0;
		}

		nr -= rv;
		pline += rv;

	} while (nr);

	return 1;
}

int send_email (char* host, char* to, char* from, char* msg) {
	int port_set = 0;
	char port[12];
	for (int i = 0; i < strlen (host); ++i) {
		if (host [i] == ':') {
			port_set = i + 1;
			strncpy (port, host + port_set, 12);
			host[i] = '\0';	
		}
	}

	if (!port_set) {
		strcpy (port, "25");
	}

	int sfd = sprocket_tcp_client (port, host);

	if (!sfd) {
		return 0;
	}


	int res = smtp_get_reply (sfd);
	if (res != 220) {
		syslog (LOG_CRIT, "SMTP reply: %d", res);
		close (sfd);
		return res;
	}

	char from_host[256];
	int host_found = 0;
	for (int i = 0; i < strlen (from); ++i) {
		if (from[i] == '@') {
			strncpy (from_host, from + (i + 1), 256);
			host_found = 1;
			break;
		}
	}

	if (!host_found) {
		syslog (LOG_CRIT, "SMTP from email format issue: %s", from);
		close (sfd);
		return 0;	
	}

	char line[512];
	snprintf (line, 512, "HELO %s\r\n", from_host);

	if (!smtp_send (sfd, line)) {
		close (sfd);
		return 0;
	}

	res = smtp_get_reply (sfd);
	if (res != 250) {
		syslog (LOG_CRIT, "SMTP reply: %d", res);
		close (sfd);
		return res;
	}
	
	snprintf (line, 512, "MAIL FROM:<%s>\r\n", from);
	if (!smtp_send (sfd, line)) {
		close (sfd);
		return 0;
	}
		
	res = smtp_get_reply (sfd);
	if (res != 250) {
		syslog (LOG_CRIT, "SMTP reply: %d", res);
		close (sfd);
		return res;
	}

	snprintf (line, 512, "RCPT TO:<%s>\r\n", to);
	if (!smtp_send (sfd, line)) {
		close (sfd);
		return 0;
	}
		
	res = smtp_get_reply (sfd);
	if (res != 250 && res != 251) {
		syslog (LOG_CRIT, "SMTP reply: %d", res);
		close (sfd);
		return res;
	}

	strcpy (line, "DATA\r\n");
	if (!smtp_send (sfd, line)) {
		close (sfd);
		return 0;
	}
		
	res = smtp_get_reply (sfd);
	if (res != 354) {
		syslog (LOG_CRIT, "SMTP reply: %d", res);
		close (sfd);
		return res;
	}

	if (!smtp_send (sfd, msg)) {
		close (sfd);
		return 0;
	}
		
	res = smtp_get_reply (sfd);
	if (res != 250) {
		syslog (LOG_CRIT, "SMTP reply: %d", res);
		close (sfd);
		return res;
	}

	strcpy (line, "QUIT\r\n");
	if (!smtp_send (sfd, line)) {
		close (sfd);
		return 0;
	}
		
	res = smtp_get_reply (sfd);
	close (sfd);

	return res;
}

