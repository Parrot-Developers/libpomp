/**
 * @file sys_timerd.h
 *
 * @brief Stub for toolchains missing sys/eventfd.h under linux.
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

#ifndef _SYS_TIMERFD_H_
#define _SYS_TIMERFD_H_

#include <sys/syscall.h>
#include <linux/fcntl.h>

#ifndef __NR_timerfd_create
	#error __NR_timerfd_create not defined !
#endif

/* timer fd defines */
#define TFD_TIMER_ABSTIME (1 << 0)
#define TFD_CLOEXEC O_CLOEXEC
#define TFD_NONBLOCK O_NONBLOCK
static inline int timerfd_create(int clockid, int flags)
{
	return syscall(__NR_timerfd_create, clockid, flags);
}

static inline int timerfd_settime(int fd, int flags,
		const struct itimerspec *new_value,
		struct itimerspec *old_value)
{
	return syscall(__NR_timerfd_settime, fd, flags, new_value, old_value);
}

static inline int timerfd_gettime(int fd, struct itimerspec *curr_value)
{
	return syscall(__NR_timerfd_gettime, fd, curr_value);
}

#endif /* _SYS_TIMERFD_H_ */
