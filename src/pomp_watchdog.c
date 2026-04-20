/**
 * @file pomp_watchdog.c
 *
 * @brief Loop watchdog.
 *
 * @author yves-marie.morgan@parrot.com
 *
 * Copyright (c) 2021 Parrot Drones SAS.
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

#ifdef POMP_HAVE_WATCHDOG

static int check_timeout(const struct timespec *timeout)
{
	struct timespec now;
	int64_t delta_ns;
	clock_gettime(CLOCK_MONOTONIC, &now);

	/* Make sure that the timeout really expired */
	delta_ns = (int64_t)(now.tv_sec - timeout->tv_sec) * 1000000000 +
			(int64_t)(now.tv_nsec - timeout->tv_nsec);
	return delta_ns >= 0;
}

static void get_absolute_timeout(uint32_t delay, struct timespec *timeout)
{
	clock_gettime(CLOCK_MONOTONIC, timeout);

	timeout->tv_sec += delay / 1000;
	timeout->tv_nsec += (delay % 1000) * 1000000;
	while (timeout->tv_nsec >= 1000000000) {
		/* over one billion nsec, add 1 sec */
		timeout->tv_sec++;
		timeout->tv_nsec -= 1000000000;
	}
}

static void notify_watchdog_expired(struct pomp_watchdog *watchdog)
{
	POMP_LOGE("Watchdog on loop=%p expired", watchdog->loop);
	(*watchdog->cb)(watchdog->loop, watchdog->userdata);
}

static void *pomp_watchdog_thread_cb(void *userdata)
{
	int res = 0;
	struct pomp_watchdog *watchdog = userdata;
	struct timespec timeout = {0, 0};
	uint32_t counter = 0;

#if defined(__APPLE__)
	#if !TARGET_OS_IPHONE
	res = pthread_setname_np("pomp_watchdog");
	if (res != 0)
		POMP_LOG_ERRNO("pthread_setname_np");
	#endif
#else
	res = pthread_setname_np(pthread_self(), "pomp_watchdog");
	if (res != 0)
		POMP_LOG_ERRNO("pthread_setname_np");
#endif

	pthread_mutex_lock(&watchdog->mutex);

	while (!watchdog->should_stop) {
		/* Do we have an absolute timeout configured ? */
		counter = watchdog->counter;
		timeout = watchdog->next_timeout;
		if (!watchdog->monitoring) {
			/* Infinite wait, on wakeup recheck if monitoring with
			 * a timeout */
			pthread_cond_wait(&watchdog->cond, &watchdog->mutex);
		} else {
			res = pthread_cond_timedwait(&watchdog->cond,
					&watchdog->mutex, &timeout);
			/* In case of timeout, check if still monitoring and
			 * in the same critical block (counter) */
			if (res == ETIMEDOUT &&
					watchdog->monitoring &&
					watchdog->counter == counter &&
					check_timeout(&timeout)) {
				notify_watchdog_expired(watchdog);
				watchdog->monitoring = 0;
			}
		}
	}

	pthread_mutex_unlock(&watchdog->mutex);

	return NULL;
}

int pomp_watchdog_start(struct pomp_watchdog *watchdog,
	struct pomp_loop *loop,
	uint32_t delay,
	pomp_watchdog_cb_t cb,
	void *userdata)
{
	int res = 0;
	pthread_condattr_t condattr;
	POMP_RETURN_ERR_IF_FAILED(watchdog != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(delay > 0, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(cb != NULL, -EINVAL);

	if (watchdog->started) {
		/* If already started, should be the same loop, update delay */
		POMP_RETURN_ERR_IF_FAILED(watchdog->loop == loop, -EBUSY);
		pthread_mutex_lock(&watchdog->mutex);
		watchdog->delay = delay;
		watchdog->cb = cb;
		watchdog->userdata = userdata;
		pthread_mutex_unlock(&watchdog->mutex);
		return 0;
	}
	POMP_RETURN_ERR_IF_FAILED(watchdog->loop == NULL, -EBUSY);

	pthread_mutex_init(&watchdog->mutex, NULL);

	pthread_condattr_init(&condattr);
#ifdef POMP_HAVE_WATCHDOG_MONOTONIC
	pthread_condattr_setclock(&condattr, CLOCK_MONOTONIC);
#endif /* POMP_HAVE_WATCHDOG_MONOTONIC */
	pthread_cond_init(&watchdog->cond, &condattr);
	pthread_condattr_destroy(&condattr);

	/* Create an internal thread */
	res = pthread_create(&watchdog->thread, NULL,
			&pomp_watchdog_thread_cb, watchdog);
	if (res != 0) {
		POMP_LOGE("pthread_create:err=%d(%s)", res, strerror(res));
		res = -res;
		goto error;
	}
	watchdog->started = 1;

	watchdog->loop = loop;
	watchdog->delay = delay;
	watchdog->cb = cb;
	watchdog->userdata = userdata;

	return 0;

	/* Cleanup in case of error */
error:
	pomp_watchdog_stop(watchdog);
	return res;
}

int pomp_watchdog_stop(struct pomp_watchdog *watchdog)
{
	POMP_RETURN_ERR_IF_FAILED(watchdog != NULL, -EINVAL);

	/* Stop and join internal thread */
	if (watchdog->started) {
		pthread_mutex_lock(&watchdog->mutex);
		watchdog->should_stop = 1;
		pthread_cond_signal(&watchdog->cond);
		pthread_mutex_unlock(&watchdog->mutex);

		pthread_join(watchdog->thread, NULL);
		memset(&watchdog->thread, 0, sizeof(watchdog->thread));
		pthread_mutex_lock(&watchdog->mutex);
		watchdog->should_stop = 0;
		pthread_mutex_unlock(&watchdog->mutex);
		watchdog->started = 0;
	}

	pthread_mutex_destroy(&watchdog->mutex);
	pthread_cond_destroy(&watchdog->cond);

	watchdog->loop = NULL;
	watchdog->delay = 0;
	watchdog->cb = NULL;
	watchdog->userdata = NULL;

	return 0;
}

void pomp_watchdog_enter(struct pomp_watchdog *watchdog)
{
	POMP_RETURN_IF_FAILED(watchdog != NULL, -EINVAL);
	if (watchdog->started) {
		pthread_mutex_lock(&watchdog->mutex);
		watchdog->counter++;
		watchdog->monitoring = 1;
		get_absolute_timeout(watchdog->delay, &watchdog->next_timeout);
		pthread_cond_signal(&watchdog->cond);
		pthread_mutex_unlock(&watchdog->mutex);
	}
}

void pomp_watchdog_leave(struct pomp_watchdog *watchdog)
{
	POMP_RETURN_IF_FAILED(watchdog != NULL, -EINVAL);
	if (watchdog->started) {
		pthread_mutex_lock(&watchdog->mutex);
		watchdog->monitoring = 0;
		watchdog->next_timeout.tv_sec = 0;
		watchdog->next_timeout.tv_nsec = 0;
		pthread_cond_signal(&watchdog->cond);
		pthread_mutex_unlock(&watchdog->mutex);
	}
}

#else /* !POMP_HAVE_WATCHDOG */

int pomp_watchdog_start(struct pomp_watchdog *watchdog,
	struct pomp_loop *loop,
	uint32_t delay,
	pomp_watchdog_cb_t cb,
	void *userdata)
{
	return -ENOSYS;
}

int pomp_watchdog_stop(struct pomp_watchdog *watchdog)
{
	return -ENOSYS;
}

void pomp_watchdog_enter(struct pomp_watchdog *watchdog)
{
}

void pomp_watchdog_leave(struct pomp_watchdog *watchdog)
{
}

#endif /* !POMP_HAVE_WATCHDOG */

void pomp_watchdog_init(struct pomp_watchdog *watchdog)
{
	POMP_RETURN_IF_FAILED(watchdog != NULL, -EINVAL);
	memset(watchdog, 0, sizeof(*watchdog));
}

void pomp_watchdog_clear(struct pomp_watchdog *watchdog)
{
	POMP_RETURN_IF_FAILED(watchdog != NULL, -EINVAL);
	pomp_watchdog_stop(watchdog);
	memset(watchdog, 0, sizeof(*watchdog));
}
