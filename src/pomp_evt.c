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
	return (*s_pomp_evt_ops->event_destroy)(evt);
}

/*
 * See documentation in public header.
 */
intptr_t pomp_evt_get_fd(const struct pomp_evt *evt)
{
	return (*s_pomp_evt_ops->event_get_fd)(evt);
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
