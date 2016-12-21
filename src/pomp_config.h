/**
 * @file pomp_config.h
 *
 * @brief platform support detection.
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

#ifndef _POMP_CONFIG_H_
#define _POMP_CONFIG_H_

#ifdef HAVE_CONFIG_H

#include "config.h"

#else /* !HAVE_CONFIG_H */

#ifdef __linux__
#  ifndef HAVE_NETDB_H
#    define HAVE_NETDB_H
#  endif
#  ifndef HAVE_SYS_EPOLL_H
#    define HAVE_SYS_EPOLL_H
#  endif
#  ifndef HAVE_SYS_EVENTFD_H
#    ifndef ANDROID_NDK
#      define HAVE_SYS_EVENTFD_H
#    endif
#  endif
#  ifndef HAVE_SYS_PARAM_H
#    define HAVE_SYS_PARAM_H
#  endif
#  ifndef HAVE_SYS_POLL_H
#    define HAVE_SYS_POLL_H
#  endif
#  ifndef HAVE_SYS_SOCKET_H
#    define HAVE_SYS_SOCKET_H
#  endif
#  ifndef HAVE_SYS_TIMERFD_H
#    ifndef ANDROID_NDK
#      define HAVE_SYS_TIMERFD_H
#    endif
#  endif
#  ifndef HAVE_SYS_UN_H
#    define HAVE_SYS_UN_H
#  endif
#  ifndef HAVE_NETINET_TCP_H
#    define HAVE_NETINET_TCP_H
#  endif
#endif

#if defined(__FreeBSD__) || defined(__APPLE__)
#  ifndef HAVE_SYS_EVENT_H
#    define HAVE_SYS_EVENT_H
#  endif
#  ifndef HAVE_NETDB_H
#    define HAVE_NETDB_H
#  endif
#  ifndef HAVE_SYS_PARAM_H
#    define HAVE_SYS_PARAM_H
#  endif
#  ifndef HAVE_SYS_POLL_H
#    define HAVE_SYS_POLL_H
#  endif
#  ifndef HAVE_SYS_SOCKET_H
#    define HAVE_SYS_SOCKET_H
#  endif
#  ifndef HAVE_SYS_UN_H
#    define HAVE_SYS_UN_H
#  endif
#  ifndef HAVE_NETINET_TCP_H
#    define HAVE_NETINET_TCP_H
#  endif
#endif

#if defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE >= 199309L)
#  ifndef HAVE_TIMER_CREATE
#    define HAVE_TIMER_CREATE
#  endif
#endif

#endif /* !HAVE_CONFIG_H */

#endif /* !_POMP_CONFIG_H_ */
