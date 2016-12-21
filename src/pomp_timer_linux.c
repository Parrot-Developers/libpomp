/**
 * @file pomp_timer_linux.c
 *
 * @brief Timer implementation, linux 'timerfd' implementation.
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

#ifdef POMP_HAVE_TIMER_FD

/**
 * Function called when the timer fd is ready for events.
 * @param fd : triggered fd.
 * @param revents : event that occurred.
 * @param userdata : timer object.
 */
static void pomp_timer_fd_cb(int fd, uint32_t revents, void *userdata)
{
	struct pomp_timer *timer = userdata;
	ssize_t res = 0;
	uint64_t val = 0;

	/* Read timer value */
	do {
		res = read(timer->tfd, &val, sizeof(val));
	} while (res < 0 && errno == EINTR);

	/* Notify callback */
	(*timer->cb)(timer, timer->userdata);
}

/**
 * @see pomp_timer_destroy.
 */
static int pomp_timer_fd_destroy(struct pomp_timer *timer)
{
	POMP_RETURN_ERR_IF_FAILED(timer != NULL, -EINVAL);

	/* Free resources */
	if (timer->tfd >= 0) {
		pomp_loop_remove(timer->loop, timer->tfd);
		close(timer->tfd);
	}
	free(timer);
	return 0;
}

/**
 * @see pomp_timer_new.
 */
static struct pomp_timer *pomp_timer_fd_new(struct pomp_loop *loop,
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
	timer->tfd = -1;

	/* Create timer fd */
	timer->tfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC|TFD_NONBLOCK);
	if (timer->tfd < 0) {
		POMP_LOG_ERRNO("timerfd_create");
		goto error;
	}

	/* Add it in loop */
	res = pomp_loop_add(timer->loop, timer->tfd, POMP_FD_EVENT_IN,
			&pomp_timer_fd_cb, timer);
	if (res < 0)
		goto error;

	/* Success */
	return timer;

	/* Cleanup in case of error */
error:
	if (timer != NULL)
		pomp_timer_fd_destroy(timer);
	return NULL;
}

/**
 * @see pomp_timer_set.
 */
static int pomp_timer_fd_set(struct pomp_timer *timer, uint32_t delay,
		uint32_t period)
{
	int res = 0;
	struct itimerspec newval, oldval;
	POMP_RETURN_ERR_IF_FAILED(timer != NULL, -EINVAL);

	/* Setup timeout */
	newval.it_interval.tv_sec = (time_t)(period / 1000);
	newval.it_interval.tv_nsec = (long int)((period % 1000) * 1000 * 1000);
	newval.it_value.tv_sec = (time_t)(delay / 1000);
	newval.it_value.tv_nsec = (long int)((delay % 1000) * 1000 * 1000);
	if (timerfd_settime(timer->tfd, 0, &newval, &oldval) < 0) {
		res = -errno;
		POMP_LOG_ERRNO("timerfd_settime");
	}

	return res;
}

/**
 * @see pomp_timer_clear.
 */
static int pomp_timer_fd_clear(struct pomp_timer *timer)
{
	POMP_RETURN_ERR_IF_FAILED(timer != NULL, -EINVAL);
	return pomp_timer_fd_set(timer, 0, 0);
}

/** Timer operations for 'timerfd' implementation */
const struct pomp_timer_ops pomp_timer_fd_ops = {
	.timer_new = &pomp_timer_fd_new,
	.timer_destroy = &pomp_timer_fd_destroy,
	.timer_set = &pomp_timer_fd_set,
	.timer_clear = &pomp_timer_fd_clear,
};

#endif /* POMP_HAVE_TIMER_FD */
