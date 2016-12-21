/**
 * @file pomp_timer_kqueue.c
 *
 * @brief Timer implementation, bsd 'kqueue' implementation.
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

#ifdef POMP_HAVE_TIMER_KQUEUE

/**
 * Function called when the timer kqueue is ready for events.
 * @param fd : triggered fd.
 * @param revents : event that occurred.
 * @param userdata : timer object.
 */
static void pomp_timer_kqueue_cb(int fd, uint32_t revents, void *userdata)
{
	struct pomp_timer *timer = userdata;
	int res = 0;
	struct kevent event;
	struct timespec timeout;

	/* Read kqueue event */
	memset(&event, 0, sizeof(event));
	memset(&timeout, 0, sizeof(timeout));
	res = kevent(timer->kq, NULL, 0, &event, 1, &timeout);
	if (res < 0) {
		POMP_LOG_ERRNO("kevent");
	} else if (res == 1) {
		/* Setup periodic timer */
		if (timer->period != 0) {
			memset(&event, 0, sizeof(event));
			event.ident = (uintptr_t)timer;
			event.filter = EVFILT_TIMER;
			event.flags = EV_ADD;
			event.fflags = NOTE_USECONDS;
			event.data = timer->period * 1000;

			/* Add timer */
			if (kevent(timer->kq, &event, 1, NULL, 0, NULL) < 0)
				POMP_LOG_ERRNO("kevent");

			/* Clear period to avoid further re-arm */
			timer->period = 0;
		}

		/* Notify callback (after re-arm because it could be
		 *  destroyed by callback) */
		(*timer->cb)(timer, timer->userdata);
	}
}

/**
 * @see pomp_timer_destroy.
 */
static int pomp_timer_kqueue_destroy(struct pomp_timer *timer)
{
	POMP_RETURN_ERR_IF_FAILED(timer != NULL, -EINVAL);

	/* Free resources */
	if (timer->kq >= 0) {
		pomp_loop_remove(timer->loop, timer->kq);
		close(timer->kq);
	}
	free(timer);
	return 0;
}

/**
 * @see pomp_timer_new.
 */
static struct pomp_timer *pomp_timer_kqueue_new(struct pomp_loop *loop,
		pomp_timer_cb_t cb, void *userdata)
{
	int res = 0;
	struct pomp_timer *timer = NULL;
	POMP_RETURN_VAL_IF_FAILED(loop != NULL, -EINVAL, NULL);
	POMP_RETURN_VAL_IF_FAILED(cb != NULL, -EINVAL, NULL);

	/* Allocate timer structure */
	timer = calloc(1, sizeof(*timer));
	if (timer == NULL)
		goto error;
	timer->loop = loop;
	timer->cb = cb;
	timer->userdata = userdata;
	timer->kq = -1;

	/* Create kqueue */
	timer->kq = kqueue();
	if (timer->kq < 0) {
		POMP_LOG_ERRNO("kqueue");
		goto error;
	}

	/* Add it in loop */
	res = pomp_loop_add(timer->loop, timer->kq, POMP_FD_EVENT_IN,
			&pomp_timer_kqueue_cb, timer);
	if (res < 0)
		goto error;

	/* Success */
	return timer;

	/* Cleanup in case of error */
error:
	if (timer != NULL)
		pomp_timer_kqueue_destroy(timer);
	return NULL;
}

/**
 * @see pomp_timer_set.
 */
static int pomp_timer_kqueue_set(struct pomp_timer *timer, uint32_t delay,
		uint32_t period)
{
	int res = 0;
	struct kevent event;
	POMP_RETURN_ERR_IF_FAILED(timer != NULL, -EINVAL);

	/* Setup event structure */
	memset(&event, 0, sizeof(event));
	event.ident = (uintptr_t)timer;
	event.filter = EVFILT_TIMER;
	event.flags = EV_ADD | EV_ONESHOT;
	event.fflags = NOTE_USECONDS;
	event.data = delay * 1000;

	/* Add timer */
	if (kevent(timer->kq, &event, 1, NULL, 0, NULL) < 0) {
		res = -errno;
		POMP_LOG_ERRNO("kevent");
	} else {
		/* Remember period to re-arm timer after first trigger */
		timer->period = period;
	}

	return res;
}

/**
 * @see pomp_timer_clear.
 */
static int pomp_timer_kqueue_clear(struct pomp_timer *timer)
{
	int res = 0;
	struct kevent event;
	POMP_RETURN_ERR_IF_FAILED(timer != NULL, -EINVAL);

	/* Setup event structure */
	memset(&event, 0, sizeof(event));
	event.ident = (uintptr_t)timer;
	event.filter = EVFILT_TIMER;
	event.flags = EV_DELETE;

	/* Remove timer (ignore errors) */
	kevent(timer->kq, &event, 1, NULL, 0, NULL);
	timer->period = 0;

	return res;
}

/** Timer operations for 'kqueue' implementation */
const struct pomp_timer_ops pomp_timer_kqueue_ops = {
	.timer_new = &pomp_timer_kqueue_new,
	.timer_destroy = &pomp_timer_kqueue_destroy,
	.timer_set = &pomp_timer_kqueue_set,
	.timer_clear = &pomp_timer_kqueue_clear,
};

#endif /* POMP_HAVE_TIMER_FD */
