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

/* Include all available implementations */
#include "pomp_evt_linux.c"
#include "pomp_evt_posix.c"
#include "pomp_evt_win32.c"

/** Choose best implementation */
static const struct pomp_evt_ops *s_pomp_evt_ops =
#if defined(POMP_HAVE_EVENT_FD)
	&pomp_evt_fd_ops;
#elif defined(POMP_HAVE_EVENT_POSIX)
	&pomp_evt_posix_ops;
#elif defined(POMP_HAVE_EVENT_WIN32)
	&pomp_evt_win32_ops;
#else
#error "No event implementation available"
#endif

/**
 * For testing purposes, allow modification of event operations.
 * @param ops : new event operations.
 * @return previous event operations.
 */
const struct pomp_evt_ops *pomp_evt_set_ops(
		const struct pomp_evt_ops *ops)
{
	const struct pomp_evt_ops *prev = s_pomp_evt_ops;
	s_pomp_evt_ops = ops;
	return prev;
}

/*
 * Internal callback for pomp_evt handling.
 * See pomp_fd_event_cb_t documentation in public header.
 */
static void pomp_evt_cb(int fd, uint32_t revents, void *userdata)
{
	int res;
	struct pomp_evt *evt = userdata;

	POMP_RETURN_IF_FAILED(evt != NULL, -EINVAL);

	res = pomp_evt_clear(evt);
	POMP_RETURN_IF_FAILED(res == 0, res);

	evt->cb(evt, evt->userdata);
}

intptr_t pomp_evt_get_fd(const struct pomp_evt *evt)
{
	return (*s_pomp_evt_ops->event_get_fd)(evt);
}

/*
 * See documentation in public header.
 */
struct pomp_evt *pomp_evt_new(void)
{
	return (*s_pomp_evt_ops->event_new)();
}

/*
 * See documentation in public header.
 */
int pomp_evt_destroy(struct pomp_evt *evt)
{
	POMP_RETURN_ERR_IF_FAILED(evt != NULL, -EINVAL);

	if (evt->loop != NULL) {
		POMP_LOGW("event %p is still attached to loop %p",
			evt, evt->loop);
		return -EBUSY;
	}

	return (*s_pomp_evt_ops->event_destroy)(evt);
}

/*
 * See documentation in public header.
 */
int pomp_evt_attach_to_loop(struct pomp_evt *evt,
		struct pomp_loop *loop, pomp_evt_cb_t cb, void *userdata)
{
	int res = 0;
	int fd;
	int events = POMP_FD_EVENT_IN;
	POMP_RETURN_ERR_IF_FAILED(evt != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(cb != NULL, -EINVAL);

	/* Make sure event is not already attached */
	if (evt->loop != NULL) {
		POMP_LOGW("event %p is already attached in %s loop",
			evt, (evt->loop == loop) ? "this" : "another");
		return -EEXIST;
	}

	/* Get evt fd */
	fd = pomp_evt_get_fd(evt);
	if (fd < 0)
		return fd;

	res = pomp_loop_add(loop, fd, events, pomp_evt_cb, evt);
	if (res < 0)
		return res;
	evt->cb = cb;
	evt->userdata = userdata;
	evt->loop = loop;

	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_evt_detach_from_loop(struct pomp_evt *evt, struct pomp_loop *loop)
{
	int res = 0;
	int fd;
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(evt != NULL, -EINVAL);

	/* Make sure event is properly attached in this loop */
	if (evt->loop == NULL) {
		POMP_LOGW("event %p is not attached to any loop", evt);
		return -ENOENT;
	} else if (evt->loop != loop) {
		POMP_LOGW("event %p is not attached to this loop", evt);
		return -EINVAL;
	}

	/* Get evt fd */
	fd = pomp_evt_get_fd(evt);
	if (fd < 0)
		return fd;

	res = pomp_loop_remove(loop, fd);
	if (res < 0)
		return res;
	evt->cb = NULL;
	evt->userdata = NULL;
	evt->loop = NULL;

	return res;
}

/*
 * See documentation in public header.
 */
POMP_API int pomp_evt_is_attached(struct pomp_evt *evt, struct pomp_loop *loop)
{
	POMP_RETURN_ERR_IF_FAILED(evt != NULL, 0);

	if (loop == NULL)
		return evt->loop != NULL;
	else
		return evt->loop == loop;
}

/*
 * See documentation in public header.
 */
int pomp_evt_signal(struct pomp_evt *evt)
{
	return (*s_pomp_evt_ops->event_signal)(evt);
}

/*
 * See documentation in public header.
 */
int pomp_evt_clear(struct pomp_evt *evt)
{
	return (*s_pomp_evt_ops->event_clear)(evt);
}
