/**
 * @file pomp_timer_win32.c
 *
 * @brief Timer implementation, win32 implementation.
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

#ifdef POMP_HAVE_TIMER_WIN32

/**
 * Native timer callback.
 * @param userdata : timer object.
 * @param fired : always TRUE for timer callbacks.
 */
static void CALLBACK pomp_timer_win32_native_cb(void *userdata, BOOLEAN fired)
{
	/* Mark timer as ready and wakeup loop */
	struct pomp_timer *timer = userdata;
	timer->pfd->revents = POMP_FD_EVENT_IN;
	pomp_loop_wakeup(timer->loop);
}

/**
 * Function called when the timer is signaled.
 * @param fd : triggered fd.
 * @param revents : event that occurred.
 * @param userdata : timer object.
 */
static void pomp_timer_win32_cb(int fd, uint32_t revents, void *userdata)
{
	struct pomp_timer *timer = userdata;
	(*timer->cb)(timer, timer->userdata);
}

/**
 * @see pomp_timer_destroy.
 */
static int pomp_timer_win32_destroy(struct pomp_timer *timer)
{
	POMP_RETURN_ERR_IF_FAILED(timer != NULL, -EINVAL);

	/* Remove from loop */
	if (timer->pfd != NULL) {
		pomp_loop_remove_pfd(timer->loop, timer->pfd);
		free(timer->pfd);
	}

	/* Free resources */
	if (timer->htimer != NULL) {
		/* Wait for cancellation */
		DeleteTimerQueueTimer(NULL, timer->htimer,
				INVALID_HANDLE_VALUE);
	}

	free(timer);
	return 0;
}

/**
 * @see pomp_timer_new.
 */
static struct pomp_timer *pomp_timer_win32_new(struct pomp_loop *loop,
		pomp_timer_cb_t cb, void *userdata)
{
	struct pomp_timer *timer = NULL;
	POMP_RETURN_VAL_IF_FAILED(loop != NULL, -EINVAL, NULL);
	POMP_RETURN_VAL_IF_FAILED(cb != NULL, -EINVAL, NULL);

	/* Allocate timer structure */
	timer = calloc(1, sizeof(*timer));
	if (timer == NULL)
		return NULL;
	timer->loop = loop;
	timer->cb = cb;
	timer->userdata = userdata;

	/* Add it in loop */
	timer->pfd = pomp_loop_add_pfd(loop, (intptr_t)timer, POMP_FD_EVENT_IN,
			&pomp_timer_win32_cb, timer);
	if (timer->pfd == NULL)
		goto error;
	timer->pfd->nofd = 1;

	/* Success */
	return timer;

	/* Cleanup in case of error */
error:
	pomp_timer_win32_destroy(timer);
	return NULL;
}

/**
 * @see pomp_timer_set.
 */
static int pomp_timer_win32_set(struct pomp_timer *timer, uint32_t delay,
		uint32_t period)
{
	int res = 0;
	POMP_RETURN_ERR_IF_FAILED(timer != NULL, -EINVAL);

	/* Delete current one if needed */
	if (timer->htimer != NULL) {
		/* Wait for cancellation */
		DeleteTimerQueueTimer(NULL, timer->htimer,
				INVALID_HANDLE_VALUE);
		timer->htimer = NULL;
	}

	/* Create timer if needed */
	if (delay != 0 && !CreateTimerQueueTimer(&timer->htimer, NULL,
			&pomp_timer_win32_native_cb, timer, delay, period, 0)) {
		res = -ENOMEM;
		POMP_LOG_ERRNO("CreateTimerQueueTimer");
	}

	return res;
}

/**
 * @see pomp_timer_clear.
 */
static int pomp_timer_win32_clear(struct pomp_timer *timer)
{
	return pomp_timer_win32_set(timer, 0, 0);
}

/** Timer operations for 'win32' implementation */
const struct pomp_timer_ops pomp_timer_win32_ops = {
	.timer_new = &pomp_timer_win32_new,
	.timer_destroy = &pomp_timer_win32_destroy,
	.timer_set = &pomp_timer_win32_set,
	.timer_clear = &pomp_timer_win32_clear,
};

#endif /* POMP_HAVE_TIMER_WIN32 */
