/**
 *  Copyright (c) 2018 Parrot Drones SAS
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
 */

#include "pomp_priv.h"

#ifdef POMP_HAVE_EVENT_FD

/**
 * @see pomp_evt_destroy.
 */
static int pomp_evt_fd_destroy(struct pomp_evt *evt)
{
	POMP_RETURN_ERR_IF_FAILED(evt != NULL, -EINVAL);

	/* Free resources */
	if (evt->efd >= 0)
		close(evt->efd);
	free(evt);
	return 0;
}

/**
 * @see pomp_evt_new.
 */
static struct pomp_evt *pomp_evt_fd_new()
{
	struct pomp_evt *evt;

	/* Allocate event structure */
	evt = calloc(1, sizeof(*evt));
	if (evt == NULL)
		goto error;
	evt->efd = eventfd(0, EFD_CLOEXEC|EFD_NONBLOCK);
	if (evt->efd < 0) {
		POMP_LOG_ERRNO("eventfd");
		goto error;
	}

	return evt;

error:
	if (evt != NULL)
		pomp_evt_fd_destroy(evt);
	return NULL;
}

/**
 * @see pomp_evt_signal
 */
static int pomp_evt_fd_signal(struct pomp_evt *evt)
{
	ssize_t ret;
	uint64_t count = 1;
	POMP_RETURN_ERR_IF_FAILED(evt != NULL, -EINVAL);

	do {
		ret = write(evt->efd, &count, sizeof(count));
	} while (ret < 0 && errno == EINTR);
	if (ret < 0) {
		ret = -errno;
		/* If the error is EAGAIN, it means that the event would
		   overflow (i.e. the current value is 2^64-2). In this case,
		   the event is already in a signaled state and we don't need
		   to propagate the error to the caller */
		if (ret == -EAGAIN)
			return 0;
		return ret;
	}
	if (ret != sizeof(count))
		return -EIO;
	return 0;
}

/**
 * @see pomp_evt_clear
 */
static int pomp_evt_fd_clear(struct pomp_evt *evt)
{
	ssize_t ret;
	uint64_t count;
	POMP_RETURN_ERR_IF_FAILED(evt != NULL, -EINVAL);

	do {
		ret = read(evt->efd, &count, sizeof(count));
	} while (ret < 0 && errno == EINTR);
	if (ret < 0) {
		ret = -errno;
		if (ret == -EAGAIN)
			return 0;
		return ret;
	}
	if (ret != sizeof(count))
		return -EIO;
	return 0;
}

/**
 * Called when event is signaled
 */
static void pomp_evt_fd_cb(int fd, uint32_t revents, void *userdata)
{
	struct pomp_evt *evt = userdata;
	pomp_evt_fd_clear(evt);
	(*evt->cb)(evt, evt->userdata);
}

/**
 * @see pomp_evt_attach_to_loop
 */
static int pomp_evt_fd_attach(struct pomp_evt *evt, struct pomp_loop *loop,
		pomp_evt_cb_t cb, void *userdata)
{
	/* Add event fd in loop */
	return pomp_loop_add(loop, evt->efd, POMP_FD_EVENT_IN,
			&pomp_evt_fd_cb, evt);
}

/**
 * @see pomp_evt_detach_from_loop
 */
static int pomp_evt_fd_detach(struct pomp_evt *evt, struct pomp_loop *loop)
{
	/* Remove event fd from loop */
	return pomp_loop_remove(loop, evt->efd);
}

/** Event operations for 'eventfd' implementation */
const struct pomp_evt_ops pomp_evt_fd_ops = {
	.event_new = &pomp_evt_fd_new,
	.event_destroy = &pomp_evt_fd_destroy,
	.event_signal = &pomp_evt_fd_signal,
	.event_clear = &pomp_evt_fd_clear,
	.event_attach = &pomp_evt_fd_attach,
	.event_detach = &pomp_evt_fd_detach,
};

#endif /* POMP_HAVE_EVENT_FD */
