/**
 * @file pomp_timer.h
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

#ifndef _POMP_TIMER_H_
#define _POMP_TIMER_H_

/** Timer structure */
struct pomp_timer {
	struct pomp_loop	*loop;		/**< Associated loop */
	pomp_timer_cb_t		cb;		/**< Notification callback */
	void			*userdata;	/**< Notification user data */

#ifdef POMP_HAVE_TIMER_POSIX
	timer_t			id;		/**< Timer id */
	int			pipefds[2];	/**< Notification pipes */
#endif /* POMP_HAVE_TIMER_POSIX */

#ifdef POMP_HAVE_TIMER_FD
	int			tfd;		/**< Timer fd */
#endif /* POMP_HAVE_TIMER_FD */

#ifdef POMP_HAVE_TIMER_KQUEUE
	int			kq;		/**< kqueue */
	uint32_t		period;		/**< Pediod (in ms)*/
#endif /* POMP_HAVE_TIMER_KQUEUE */

#ifdef POMP_HAVE_TIMER_WIN32
	HANDLE			htimer;		/**< Timer handle */
	HANDLE			hevt;		/**< Notification event */
#endif /* POMP_HAVE_TIMER_WIN32 */
};

/** Timer operations */
struct pomp_timer_ops {
	/** Implementation specific 'new' operation. */
	struct pomp_timer *(*timer_new)(struct pomp_loop *loop,
			pomp_timer_cb_t cb, void *userdata);

	/** Implementation specific 'destroy' operation. */
	int (*timer_destroy)(struct pomp_timer *timer);

	/** Implementation specific 'set' operation. */
	int (*timer_set)(struct pomp_timer *timer, uint32_t delay,
			uint32_t period);

	/** Implementation specific 'clear' operation. */
	int (*timer_clear)(struct pomp_timer *timer);
};

/** Timer operations for 'posix' implementation */
#ifdef POMP_HAVE_TIMER_POSIX
extern const struct pomp_timer_ops pomp_timer_posix_ops;
#endif /* POMP_HAVE_TIMER_POSIX */

/** Timer operations for 'timerfd' implementation */
#ifdef POMP_HAVE_TIMER_FD
extern const struct pomp_timer_ops pomp_timer_fd_ops;
#endif /* POMP_HAVE_TIMER_FD */

/** Timer operations for 'kqueue' implementation */
#ifdef POMP_HAVE_TIMER_KQUEUE
extern const struct pomp_timer_ops pomp_timer_kqueue_ops;
#endif /* POMP_HAVE_TIMER_KQUEUE */

/** Timer operations for 'win32' implementation */
#ifdef POMP_HAVE_TIMER_WIN32
extern const struct pomp_timer_ops pomp_timer_win32_ops;
#endif /* POMP_HAVE_TIMER_WIN32 */

/* Timer functions not part of public API */

const struct pomp_timer_ops *pomp_timer_set_ops(
		const struct pomp_timer_ops *ops);

#endif /* !_POMP_TIMER_H_ */
