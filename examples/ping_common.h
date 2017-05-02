/**
 * @file ping_common.h
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

#ifndef _PING_COMMON_H_
#define _PING_COMMON_H_

/* Win32 headers */
#ifdef _WIN32
#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT 0x0501
#  endif /* !_WIN32_WINNT */
#  include <winsock2.h>
#  include <process.h>
#endif /* _WIN32 */

/* Standard headers */
#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif /* !_GNU_SOURCE */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

/* Unix headers */
#ifndef _WIN32
#  include <unistd.h>
#  include <sys/socket.h>
#endif /* !_WIN32 */

#include "libpomp.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Win32 stubs */
#ifdef _WIN32
/* codecheck_ignore[NEW_TYPEDEFS] */
typedef unsigned int	uid_t;		/**< User identification */
/* codecheck_ignore[NEW_TYPEDEFS] */
typedef unsigned int	gid_t;		/**< Group identification */
static inline uid_t getuid() { return 0; }
static inline gid_t getgid() { return 0; }
static inline const char *strsignal(int signum) { return "??"; }
#endif /* _WIN32 */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_PING_COMMON_H_ */
