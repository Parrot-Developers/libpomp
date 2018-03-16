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
	return -ENOSYS;
}

/**
 * @see pomp_evt_new.
 */
static struct pomp_evt *pomp_evt_win32_new()
{
	return NULL;
}

/**
 * @see pomp_evt_get_fd
 */
static intptr_t pomp_evt_win32_get_fd(const struct pomp_evt *evt)
{
	return -ENOSYS;
}

/**
 * @see pomp_evt_signal
 */
static int pomp_evt_win32_signal(struct pomp_evt *evt)
{
	return -ENOSYS;
}

/**
 * @see pomp_evt_clear
 */
static int pomp_evt_win32_clear(struct pomp_evt *evt)
{
	return -ENOSYS;
}

/** Event operations for 'eventfd' implementation */
const struct pomp_evt_ops pomp_evt_win32_ops = {
	.event_new = &pomp_evt_win32_new,
	.event_destroy = &pomp_evt_win32_destroy,
	.event_get_fd = &pomp_evt_win32_get_fd,
	.event_signal = &pomp_evt_win32_signal,
	.event_clear = &pomp_evt_win32_clear,
};

#endif /* POMP_HAVE_EVENT_WIN32 */