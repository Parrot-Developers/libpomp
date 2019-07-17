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

#ifndef _POMP_EVT_H_
#define _POMP_EVT_H_

#include <stdint.h>

/** Event structure */
struct pomp_evt {
	struct pomp_loop *loop;
	pomp_evt_cb_t cb;
	void *userdata;

#ifdef POMP_HAVE_EVENT_POSIX
	int		pipefds[2];	/**< Notification pipes */
#endif /* POMP_HAVE_EVENT_POSIX */

#ifdef POMP_HAVE_EVENT_FD
	int		efd;		/**< Event fd */
#endif /* POMP_HAVE_EVENT_FD */

#ifdef POMP_HAVE_EVENT_WIN32
	struct pomp_fd	*pfd;
	int		signaled;	/**< Event is signaled */
#endif /* POMP_HAVE_EVENT_WIN32 */
};

/** Event operations */
struct pomp_evt_ops {
	/** Implementation specific 'new' operation. */
	struct pomp_evt *(*event_new)();

	/** Implementation specific 'destroy' operation. */
	int (*event_destroy)(struct pomp_evt *evt);

	/** Implementation specific 'signal' operation. */
	int (*event_signal)(struct pomp_evt *evt);

	/** Implementation specific 'clear' operation. */
	int (*event_clear)(struct pomp_evt *evt);

	/** Implementation specific 'attach' operation. */
	int (*event_attach)(struct pomp_evt *evt, struct pomp_loop *loop,
			pomp_evt_cb_t cb, void *userdata);

	/** Implementation specific 'detach' operation. */
	int (*event_detach)(struct pomp_evt *evt, struct pomp_loop *loop);
};

/** Event operations for 'posix' implementation */
#ifdef POMP_HAVE_EVENT_POSIX
extern const struct pomp_evt_ops pomp_evt_posix_ops;
#endif /* POMP_HAVE_EVENT_POSIX */

/** Event operations for 'eventfd' implementation */
#ifdef POMP_HAVE_EVENT_FD
extern const struct pomp_evt_ops pomp_evt_fd_ops;
#endif /* POMP_HAVE_EVENT_FD */

/** Event operations for 'win32' implementation */
#ifdef POMP_HAVE_EVENT_WIN32
extern const struct pomp_evt_ops pomp_evt_win32_ops;
#endif /* POMP_HAVE_EVENT_WIN32 */

/* Event functions not part of public API */

const struct pomp_evt_ops *pomp_evt_set_ops(
		const struct pomp_evt_ops *ops);

#endif /* !_POMP_EVT_H_ */
