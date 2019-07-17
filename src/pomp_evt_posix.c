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

#ifdef POMP_HAVE_EVENT_POSIX

/**
 * @see pomp_evt_destroy.
 */
static int pomp_evt_posix_destroy(struct pomp_evt *evt)
{
	POMP_RETURN_ERR_IF_FAILED(evt != NULL, -EINVAL);

	/* Free resources */
	if (evt->pipefds[0] >= 0)
		close(evt->pipefds[0]);
	if (evt->pipefds[1] >= 0)
		close(evt->pipefds[1]);
	free(evt);
	return 0;
}

/**
 * @see pomp_evt_new.
 */
static struct pomp_evt *pomp_evt_posix_new()
{
	int res;
	struct pomp_evt *evt;

	/* Allocate event structure */
	evt = calloc(1, sizeof(*evt));
	if (evt == NULL)
		goto error;

	evt->pipefds[0] = -1;
	evt->pipefds[1] = -1;

	res = pipe(evt->pipefds);
	if (res < 0) {
		POMP_LOG_ERRNO("pipe");
		goto error;
	}

	res = fd_setup_flags(evt->pipefds[0]);
	if (res < 0)
		goto error;
	res = fd_setup_flags(evt->pipefds[1]);
	if (res < 0)
		goto error;

	return evt;

error:
	if (evt != NULL)
		pomp_evt_posix_destroy(evt);
	return NULL;
}

/**
 * @see pomp_evt_signal
 */
static int pomp_evt_posix_signal(struct pomp_evt *evt)
{
	ssize_t ret;
	uint8_t dummy = 0;
	POMP_RETURN_ERR_IF_FAILED(evt != NULL, -EINVAL);

	do {
		ret = write(evt->pipefds[1], &dummy, sizeof(dummy));
	} while (ret < 0 && errno == EINTR);
	if (ret < 0) {
		ret = -errno;
		/* If the error is EAGAIN, it means that the pipe buffer is
		   full. In this case, the event is already in a signaled state
		   and we don't need to propagate the error to the caller */
		if (ret == -EAGAIN)
			return 0;
		return ret;
	}
	if (ret != sizeof(dummy))
		return -EIO;
	return 0;
}

/**
 * @see pomp_evt_clear
 */
static int pomp_evt_posix_clear(struct pomp_evt *evt)
{
	ssize_t ret;
	uint8_t dummy[64];
	POMP_RETURN_ERR_IF_FAILED(evt != NULL, -EINVAL);

	while (1) {
		do {
			ret = read(evt->pipefds[0], dummy, sizeof(dummy));
		} while (ret < 0 && errno == EINTR);
		if (ret < 0) {
			ret = -errno;
			if (ret == -EAGAIN)
				return 0;
			return ret;
		}
		if ((size_t)ret < sizeof(dummy))
			return 0;
	}
}

/**
 * Called when event is signaled
 */
static void pomp_evt_posix_cb(int fd, uint32_t revents, void *userdata)
{
	struct pomp_evt *evt = userdata;
	pomp_evt_posix_clear(evt);
	(*evt->cb)(evt, evt->userdata);
}

/**
 * @see pomp_evt_attach_to_loop
 */
static int pomp_evt_posix_attach(struct pomp_evt *evt, struct pomp_loop *loop,
		pomp_evt_cb_t cb, void *userdata)
{
	/* Add read pipe fd in loop */
	return pomp_loop_add(loop, evt->pipefds[0], POMP_FD_EVENT_IN,
			&pomp_evt_posix_cb, evt);
}

/**
 * @see pomp_evt_detach_from_loop
 */
static int pomp_evt_posix_detach(struct pomp_evt *evt, struct pomp_loop *loop)
{
	/* Remove read pipe fd from loop */
	return pomp_loop_remove(loop, evt->pipefds[0]);
}

/** Event operations for 'posix' implementation */
const struct pomp_evt_ops pomp_evt_posix_ops = {
	.event_new = &pomp_evt_posix_new,
	.event_destroy = &pomp_evt_posix_destroy,
	.event_signal = &pomp_evt_posix_signal,
	.event_clear = &pomp_evt_posix_clear,
	.event_attach = &pomp_evt_posix_attach,
	.event_detach = &pomp_evt_posix_detach,
};

#endif /* POMP_HAVE_EVENT_POSIX */