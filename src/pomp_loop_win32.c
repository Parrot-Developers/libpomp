/**
 * @file pomp_loop_win32.c
 *
 * @brief Event loop, win32 implementation.
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

#ifdef POMP_HAVE_LOOP_WIN32

/**
 * Convert fd event from wsa events.
 * @param events : wsa events.
 * @return fd events.
 */
static uint32_t fd_events_from_wsa(long events)
{
	uint32_t res = 0;
	if (events & FD_READ)
		res |= POMP_FD_EVENT_IN;
	if (events & FD_WRITE)
		res |= POMP_FD_EVENT_OUT;
	if (events & FD_ACCEPT)
		res |= POMP_FD_EVENT_IN;
	if (events & FD_CONNECT)
		res |= POMP_FD_EVENT_OUT;
	if (events & FD_CLOSE)
		res |= POMP_FD_EVENT_IN;
	return res;
}

/**
 * Convert fd event to wsa events.
 * @param events : fd events.
 * @return wsa events.
 */
static long fd_events_to_wsa(uint32_t events)
{
	long res = 0;
	if (events & POMP_FD_EVENT_IN)
		res |= FD_READ | FD_ACCEPT | FD_CLOSE;
	if (events & POMP_FD_EVENT_OUT)
		res |= FD_WRITE | FD_CONNECT;
	return res;
}

/**
 * @see pomp_loop_do_destroy.
 */
static int pomp_loop_win32_do_destroy(struct pomp_loop *loop)
{
	/* Cleanup wakeup event */
	if (loop->wakeup.hevt != NULL) {
		CloseHandle(loop->wakeup.hevt);
		loop->wakeup.hevt = NULL;
	}

	/* Cleanup winsock API */
	WSACleanup();
	return 0;
}

/**
 * @see pomp_loop_do_new.
 */
static int pomp_loop_win32_do_new(struct pomp_loop *loop)
{
	int res = 0;
	WSADATA wsadata;

	/* Initialize winsock API */
	if (WSAStartup(MAKEWORD(2, 0), &wsadata) != 0) {
		POMP_LOGE("WSAStartup error");
		return -ENOMEM;
	}

	/* Create wakeup event (auto reset) */
	loop->wakeup.hevt = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (loop->wakeup.hevt == NULL) {
		res = -ENOMEM;
		POMP_LOG_ERRNO("CreateEvent");
		goto error;
	}

	/* Success */
	return 0;

	/* Cleanup in case of error */
error:
	pomp_loop_win32_do_destroy(loop);
	return res;
}

/**
 * @see pomp_loop_do_add.
 */
static int pomp_loop_win32_do_add(struct pomp_loop *loop, struct pomp_fd *pfd)
{
	int res = 0;
	long wsaevents = fd_events_to_wsa(pfd->events);

	/* Setup monitor */
	if (WSAEventSelect(pfd->fd, loop->wakeup.hevt, wsaevents) != 0) {
		res = -errno;
		POMP_LOG_ERRNO("WSAEventSelect");
	}

	return res;
}

/**
 * @see pomp_loop_do_update.
 */
static int pomp_loop_win32_do_update(struct pomp_loop *loop,
		struct pomp_fd *pfd)
{
	int res = 0;
	long wsaevents = fd_events_to_wsa(pfd->events);

	/* Update monitor */
	if (WSAEventSelect(pfd->fd, loop->wakeup.hevt, wsaevents) != 0) {
		res = -errno;
		POMP_LOG_ERRNO("WSAEventSelect");
	}

	return res;
}

/**
 * @see pomp_loop_do_remove.
 */
static int pomp_loop_win32_do_remove(struct pomp_loop *loop,
		struct pomp_fd *pfd)
{
	int res = 0;

	/* Clear monitor */
	if (WSAEventSelect(pfd->fd, loop->wakeup.hevt, 0) != 0) {
		res = -errno;
		POMP_LOG_ERRNO("WSAEventSelect");
	}

	return res;
}

/**
 * @see pomp_loop_do_get_fd.
 */
static intptr_t pomp_loop_win32_do_get_fd(struct pomp_loop *loop)
{
	return (intptr_t)loop->wakeup.hevt;
}

/**
 * @see pomp_loop_do_wait_and_process.
 */
static int pomp_loop_win32_do_wait_and_process(struct pomp_loop *loop,
		int timeout)
{
	int res = 0;
	DWORD waitres = 0;
	struct pomp_fd *pfd = NULL;
	unsigned int pfdi;
	WSANETWORKEVENTS events;
	uint32_t revents = 0;

	/* Wait for an event in the loop (ready event is auto reset) */
	waitres = WaitForSingleObject(loop->wakeup.hevt,
			timeout == -1 ? INFINITE : (DWORD)timeout);
	switch (waitres) {
	case WAIT_OBJECT_0:
		break;

	case WAIT_TIMEOUT:
		/* If timeout was 0, still check for events in case the wait
		 * was done externally and the event was already cleared */
		res = -ETIMEDOUT;
		if (timeout != 0)
			goto out;
		break;

	default:
		POMP_LOGW("Unexpected wait result : %u", (uint32_t)waitres);
		goto out;
	}

	pomp_watchdog_enter(&loop->watchdog);

	for (;;) {
		/* Find a ready fd */
		for (pfdi = 0; pfdi < POMP_LOOP_PFDS_LEN; pfdi++) {
			for (pfd = loop->pfds[pfdi]; pfd != NULL; pfd = pfd->next) {
				if (!pfd->nofd) {
					memset(&events, 0, sizeof(events));
					WSAEnumNetworkEvents(pfd->fd, NULL, &events);
					if (events.lNetworkEvents != 0) {
						pfd->revents = fd_events_from_wsa(
								  events.lNetworkEvents);
						goto found;
					}
				} else if (pfd->revents != 0) {
					goto found;
				}
			}
		}

found:
		if (pfd == NULL)
			break;

		/* Save and clear revents before calling callback (pfd might be
		 * destroyed during the call */
		revents = pfd->revents;
		pfd->revents = 0;
		(*pfd->cb)(pfd->fd, revents, pfd->userdata);
		res = 0;
	}

	pomp_watchdog_leave(&loop->watchdog);

out:
	return res;
}

/**
 * @see pomp_loop_do_wakeup.
 */
static int pomp_loop_win32_do_wakeup(struct pomp_loop *loop)
{
	/* Set notification event */
	SetEvent(loop->wakeup.hevt);
	return 0;
}

/** Loop operations for win32 implementation */
const struct pomp_loop_ops pomp_loop_win32_ops = {
	.do_new = &pomp_loop_win32_do_new,
	.do_destroy = &pomp_loop_win32_do_destroy,
	.do_add = &pomp_loop_win32_do_add,
	.do_update = &pomp_loop_win32_do_update,
	.do_remove = &pomp_loop_win32_do_remove,
	.do_get_fd = &pomp_loop_win32_do_get_fd,
	.do_wait_and_process = &pomp_loop_win32_do_wait_and_process,
	.do_wakeup = &pomp_loop_win32_do_wakeup,
};

#endif /* POMP_HAVE_LOOP_WIN32 */
