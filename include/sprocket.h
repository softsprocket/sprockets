/** @file */
/**
* \mainpage 
*    **Sprockets** - server execution software components
*    ====================================================
* \code{.unparsed}   
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
* \endcode
*/

#ifndef SPROCKETS_H_
#define SPROCKETS_H_

#ifndef SOFTSPROCKET_CONTAINER_HEADER_INCLUDED_
#define SOFTSPROCKET_CONTAINER_HEADER_INCLUDED_
#include <softsprocket/container.h>
#endif // SOFTSPROCKET_CONTAINER_HEADER_INCLUDED_

int exec (hash_table* config);

int sprocket_bus_server (char* port, char* host);

int sprocket_bus_client (char* port, char* host);

int init_epoll (int fd);

int add_fd_to_epoll (int epfd, int fd);

#endif // SPROCKETS_H_

