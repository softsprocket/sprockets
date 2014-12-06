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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>

#include <softsprocket/container.h>
#include <softsprocket/serc.h>

#include "sprocket.h"

int exec (hash_table* config) {
	syslog (LOG_NOTICE, "Server0 running\n");
	int sfd = sprocket_bus_server ("10000", NULL);
	int epfd = init_epoll (sfd);

	close (epfd);
	close (sfd);
	syslog (LOG_NOTICE, "Server0 exiting\n");
	return EXIT_SUCCESS;
}

