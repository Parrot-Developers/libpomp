/**
 * @file pomp_timer.c
 *
 * @brief Timer implementation.
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

/* Include all available implementations */
#include "pomp_timer_kqueue.c"
#include "pomp_timer_linux.c"
#include "pomp_timer_posix.c"
#include "pomp_timer_win32.c"

/** Choose best implementation */
static const struct pomp_timer_ops *s_pomp_timer_ops =
#if defined(POMP_HAVE_TIMER_FD)
	&pomp_timer_fd_ops;
#elif defined(POMP_HAVE_TIMER_KQUEUE)
	&pomp_timer_kqueue_ops;
#elif defined(POMP_HAVE_TIMER_POSIX)
	&pomp_timer_posix_ops;
#elif defined(POMP_HAVE_TIMER_WIN32)
	&pomp_timer_win32_ops;
#else
#error "No timer implementation available"
#endif

/**
 * For testing purposes, allow modification of timer operations.
 * @param ops : new timer operations.
 * @return previous timer operations.
 */
const struct pomp_timer_ops *pomp_timer_set_ops(
		const struct pomp_timer_ops *ops)
{
	const struct pomp_timer_ops *prev = s_pomp_timer_ops;
	s_pomp_timer_ops = ops;
	return prev;
}

/*
 * See documentation in public header.
 */
struct pomp_timer *pomp_timer_new(struct pomp_loop *loop,
		pomp_timer_cb_t cb, void *userdata)
{
	return (*s_pomp_timer_ops->timer_new)(loop, cb, userdata);
}

/*
 * See documentation in public header.
 */
int pomp_timer_destroy(struct pomp_timer *timer)
{
	return (*s_pomp_timer_ops->timer_destroy)(timer);
}

/*
 * See documentation in public header.
 */
int pomp_timer_set(struct pomp_timer *timer, uint32_t delay)
{
	return (*s_pomp_timer_ops->timer_set)(timer, delay, 0);
}

/*
 * See documentation in public header.
 */
int pomp_timer_set_periodic(struct pomp_timer *timer, uint32_t delay,
		uint32_t period)
{
	return (*s_pomp_timer_ops->timer_set)(timer, delay, period);
}

/*
 * See documentation in public header.
 */
int pomp_timer_clear(struct pomp_timer *timer)
{
	return (*s_pomp_timer_ops->timer_clear)(timer);
}
