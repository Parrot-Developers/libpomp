/**
 * @file pomp_log.c
 *
 * @brief Log functions and macros.
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

#include "pomp_priv.h"

/* ulog requires 1 source file to declare the log tag */
#ifdef BUILD_LIBULOG
ULOG_DECLARE_TAG(pomp);
#endif /* BUILD_LIBULOG */

#ifdef _WIN32

int pomp_win32_error_to_errno(int error)
{
	switch (error) {
	case 0: return 0;
	case ERROR_ACCESS_DENIED: return EACCES;
	case ERROR_ALREADY_EXISTS: return EEXIST;
	case ERROR_DISK_FULL: return ENOSPC;
	case ERROR_FILE_EXISTS: return EEXIST;
	case ERROR_FILE_NOT_FOUND: return ENOENT;
	case ERROR_NOT_ENOUGH_MEMORY: return ENOMEM;

	case WSAEINTR: return EINTR;
	case WSAEBADF: return EBADF;
	case WSAEACCES: return EACCES;
	case WSAEFAULT: return EFAULT;
	case WSAEINVAL: return EINVAL;
	case WSAEMFILE: return EMFILE;
	case WSAEWOULDBLOCK: return EWOULDBLOCK;
	case WSAEINPROGRESS: return EINPROGRESS;
	case WSAEALREADY: return EALREADY;
	case WSAENOTSOCK: return ENOTSOCK;
	case WSAEDESTADDRREQ: return EDESTADDRREQ;
	case WSAEMSGSIZE: return EMSGSIZE;
	case WSAEPROTOTYPE: return EPROTOTYPE;
	case WSAENOPROTOOPT: return ENOPROTOOPT;
	case WSAEPROTONOSUPPORT: return EPROTONOSUPPORT;
	/*case WSAESOCKTNOSUPPORT: return ESOCKTNOSUPPORT;*/
	case WSAEOPNOTSUPP: return EOPNOTSUPP;
	/*case WSAEPFNOSUPPORT: return EPFNOSUPPORT;*/
	case WSAEAFNOSUPPORT: return EAFNOSUPPORT;
	case WSAEADDRINUSE: return EADDRINUSE;
	case WSAEADDRNOTAVAIL: return EADDRNOTAVAIL;
	case WSAENETDOWN: return ENETDOWN;
	case WSAENETUNREACH: return ENETUNREACH;
	case WSAENETRESET: return ENETRESET;
	case WSAECONNABORTED: return ECONNABORTED;
	case WSAECONNRESET: return ECONNRESET;
	case WSAENOBUFS: return ENOBUFS;
	case WSAEISCONN: return EISCONN;
	case WSAENOTCONN: return ENOTCONN;
	/*case WSAESHUTDOWN: return ESHUTDOWN;*/
	/*case WSAETOOMANYREFS: return ETOOMANYREFS;*/
	case WSAETIMEDOUT: return ETIMEDOUT;
	case WSAECONNREFUSED: return ECONNREFUSED;
	case WSAELOOP: return ELOOP;
	case WSAENAMETOOLONG: return ENAMETOOLONG;
	case WSAEHOSTDOWN: return EHOSTDOWN;
	case WSAEHOSTUNREACH: return EHOSTUNREACH;
	case WSAENOTEMPTY: return ENOTEMPTY;

	default:
		POMP_LOGW("Unknown win32 error:%d", error);
		/* codecheck_ignore[USE_NEGATIVE_ERRNO] */
		return EINVAL;
	}
}

#endif /* _WIN32 */
