/**
 * @file pomp_priv_win32.h
 *
 * @brief private headers for 'win32'.
 *
 * @author yves-marie.morgan@parrot.com
 *
 * Copyright (c) 2014 Parrot S.A.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Parrot Company nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE PARROT COMPANY BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _POMP_PRIV_WIN32_H_
#define _POMP_PRIV_WIN32_H_

#ifdef _WIN32

#ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0501
#endif /* !_WIN32_WINNT */
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#  include <mstcpip.h>
#endif /* _MSC_VER */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define POMP_HAVE_TIMER_WIN32
#define POMP_HAVE_LOOP_WIN32

#ifndef SIO_KEEPALIVE_VALS
#define SIO_KEEPALIVE_VALS	_WSAIOW(IOC_VENDOR, 4)
struct tcp_keepalive {
	ULONG onoff;
	ULONG keepalivetime;
	ULONG keepaliveinterval;
};
#endif /* !SIO_KEEPALIVE_VALS */

#undef EWOULDBLOCK
#undef EADDRNOTAVAIL
#undef ENETDOWN
#undef ENETUNREACH
#undef ENOTCONN
#undef ETIMEDOUT
#undef ECONNREFUSED
#undef EHOSTDOWN
#undef EHOSTUNREACH
#undef EINPROGRESS

#define EWOULDBLOCK	WSAEWOULDBLOCK
#define EADDRNOTAVAIL	WSAEADDRNOTAVAIL
#define ENETDOWN	WSAENETDOWN
#define ENETUNREACH	WSAENETUNREACH
#define ENOTCONN	WSAENOTCONN
#define ETIMEDOUT	WSAETIMEDOUT
#define ECONNREFUSED	WSAECONNREFUSED
#define EHOSTDOWN	WSAEHOSTDOWN
#define EHOSTUNREACH	WSAEHOSTUNREACH
#define EINPROGRESS	WSAEINPROGRESS

#define SHUT_RD		SD_RECEIVE	/**< No more receptions */
#define SHUT_WR		SD_SEND		/**< No more transmissions */
#define SHUT_RDWR	SD_BOTH		/**< No more receptions or transmissions */

#define O_NONBLOCK	00004000

#define FD_CLOEXEC	1

#define F_GETFD		1		/**< Get file descriptor flags */
#define F_SETFD		2		/**< Set file descriptor flags */
#define F_GETFL		3		/**< Get file status flags */
#define F_SETFL		4		/**< Set file status flags */

#ifndef PRIi64
#  define PRIi64 "lli"
#endif /* !PRIi64 */
#ifndef PRIu64
#  define PRIu64 "llu"
#endif /* !PRIu64 */

/* Signed size_t type */
#ifdef _MSC_VER
#  ifdef _WIN64
/* codecheck_ignore[NEW_TYPEDEFS] */
typedef signed __int64  ssize_t;
#  else /* !_WIN64 */
/* codecheck_ignore[NEW_TYPEDEFS] */
typedef _W64 signed int ssize_t;
#  endif /* !_WIN64 */
#endif /* _MSC_VER */

/* codecheck_ignore[NEW_TYPEDEFS] */
typedef int		socklen_t;

struct sockaddr_un {
	short	sun_family;	/**< AF_UNIX */
	char	sun_path[108];	/**< pathname */
};

static inline int win32_close(int fd)
{
	return closesocket((SOCKET)fd);
}

static inline ssize_t win32_read(int fd, void *buf, size_t len)
{
	return (ssize_t)recv((SOCKET)fd, buf, (int)len, 0);
}

static inline ssize_t win32_recvfrom(int fd, void *buf, size_t len,
		int flags, struct sockaddr *addr, socklen_t *addrlen)
{
	return (ssize_t)recvfrom((SOCKET)fd, buf, (int)len,
			flags, addr, addrlen);
}

static inline ssize_t win32_write(int fd, const void *buf, size_t len)
{
	return (ssize_t)send((SOCKET)fd, buf, (int)len, 0);
}

static inline ssize_t win32_sendto(int fd, const void *buf, size_t len,
		int flags, const struct sockaddr *addr, socklen_t addrlen)
{
	return (ssize_t)sendto((SOCKET)fd, buf, (int)len,
			flags, addr, addrlen);
}

static inline int win32_fcntl(int fd, int cmd, ...)
{
	return 0;
}

static inline int win32_setsockopt(int sockfd, int level, int optname,
		const void *optval, socklen_t optlen)
{
	return setsockopt((SOCKET)sockfd, level, optname,
			(const char *)optval, optlen);
}

#undef close
#undef read
#undef recvfrom
#undef write
#undef sendto
#undef fcntl
#undef setsockopt
#undef errno

#define close		win32_close
#define read		win32_read
#define recvfrom	win32_recvfrom
#define write		win32_write
#define sendto		win32_sendto
#define fcntl		win32_fcntl
#define setsockopt	win32_setsockopt
#define errno		((int)GetLastError())

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _WIN32 */

#endif /* !_POMP_PRIV_WIN32_H_ */
