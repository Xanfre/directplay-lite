/* DirectPlay Lite
 * Copyright (C) 2018 Daniel Collins <solemnwarning@solemnwarning.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef DPLITE_PLATFORM_HPP
#define DPLITE_PLATFORM_HPP

#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <windows.h>
#else
	#include <sys/socket.h>
	#include <sys/types.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <unistd.h>
	#include <fcntl.h>
	#include <errno.h>
	#include <string.h>

	/* Compatibility macros */
	#define closesocket close
	#define WSAEWOULDBLOCK EWOULDBLOCK
	#define WSAEADDRINUSE EADDRINUSE
	#define WSAEINTR EINTR
	#define WSAECONNRESET ECONNRESET
	#define WSAENOTCONN ENOTCONN

	#define FD_READ    0x01
	#define FD_WRITE   0x02
	#define FD_ACCEPT  0x08
	#define FD_CONNECT 0x10
	#define FD_CLOSE   0x20

	inline int WSAGetLastError() { return errno; }

	inline const char* unix_strerror(int err) { return strerror(err); }
#endif

/* Platform-independent event signaling */
#ifdef _WIN32
	#define SIGNAL_EVENT(event) SetEvent(event)
	#define RESET_EVENT(event) ResetEvent(event)
#else
	#include "EventObject.hpp"

	#define SIGNAL_EVENT(event) (event).set()
	#define RESET_EVENT(event) (event).reset()
#endif

/* Platform-independent error string */
#ifdef _WIN32
	#define SOCKET_STRERROR(err) win_strerror(err).c_str()
#else
	#define SOCKET_STRERROR(err) unix_strerror(err)
#endif

#endif /* !DPLITE_PLATFORM_HPP */
