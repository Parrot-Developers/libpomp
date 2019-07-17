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

#ifdef POMP_HAVE_EVENT_WIN32

/**
 * @see pomp_evt_destroy.
 */
static int pomp_evt_win32_destroy(struct pomp_evt *evt)
{
	free(evt);
	return 0;
}

/**
 * @see pomp_evt_new.
 */
static struct pomp_evt *pomp_evt_win32_new()
{
	struct pomp_evt *evt = NULL;

	/* Allocate event structure */
	evt = calloc(1, sizeof(*evt));
	if (evt == NULL)
		return NULL;

	/* Nothing more to do for win32 event */
	return evt;
}

/**
 * @see pomp_evt_signal
 */
static int pomp_evt_win32_signal(struct pomp_evt *evt)
{
	POMP_RETURN_ERR_IF_FAILED(evt != NULL, -EINVAL);

	/* Set signaled state, wakeup loop if needed */
	evt->signaled = 1;
	if (evt->loop != NULL && evt->pfd != NULL) {
		evt->pfd->revents = POMP_FD_EVENT_IN;
		pomp_loop_wakeup(evt->loop);
	}

	return 0;
}

/**
 * @see pomp_evt_clear
 */
static int pomp_evt_win32_clear(struct pomp_evt *evt)
{
	POMP_RETURN_ERR_IF_FAILED(evt != NULL, -EINVAL);

	/* Clear signaled state */
	evt->signaled = 0;
	if (evt->pfd != NULL)
		evt->pfd->revents = 0;

	return 0;
}

/**
 * Called when event is signaled
 */
static void pomp_evt_win32_cb(int fd, uint32_t revents, void *userdata)
{
	struct pomp_evt *evt = userdata;
	if (evt->signaled) {
		pomp_evt_win32_clear(evt);
		(*evt->cb)(evt, evt->userdata);
	}
}

/**
 * @see pomp_evt_attach_to_loop
 */
static int pomp_evt_win32_attach(struct pomp_evt *evt, struct pomp_loop *loop,
		pomp_evt_cb_t cb, void *userdata)
{
	/* Add in loop */
	evt->pfd = pomp_loop_add_pfd(loop, (intptr_t)evt, POMP_FD_EVENT_IN,
			&pomp_evt_win32_cb, evt);
	if (evt->pfd == NULL)
		return -ENOMEM;
	evt->pfd->nofd = 1;

	/* If event is in signaled state, post message to loop */
	if (evt->signaled)
		pomp_loop_wakeup(loop);
	return 0;
}

/**
 * @see pomp_evt_detach_from_loop
 */
static int pomp_evt_win32_detach(struct pomp_evt *evt, struct pomp_loop *loop)
{
	/* Remove from loop */
	pomp_loop_remove_pfd(loop, evt->pfd);
	free(evt->pfd);
	return 0;
}

/** Event operations for 'win32' implementation */
const struct pomp_evt_ops pomp_evt_win32_ops = {
	.event_new = &pomp_evt_win32_new,
	.event_destroy = &pomp_evt_win32_destroy,
	.event_signal = &pomp_evt_win32_signal,
	.event_clear = &pomp_evt_win32_clear,
	.event_attach = &pomp_evt_win32_attach,
	.event_detach = &pomp_evt_win32_detach,
};

#endif /* POMP_HAVE_EVENT_WIN32 */