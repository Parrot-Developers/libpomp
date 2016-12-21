/**
 * @file pomp_loop_posix.c
 *
 * @brief Event loop, posix 'poll' implementation.
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

#ifdef POMP_HAVE_LOOP_POLL

/**
 * Convert fd event from poll events.
 * @param events : poll events.
 * @return fd events.
 */
static uint32_t fd_events_from_poll(int16_t events)
{
	uint32_t res = 0;
	if (events & POLLIN)
		res |= POMP_FD_EVENT_IN;
	if (events & POLLPRI)
		res |= POMP_FD_EVENT_PRI;
	if (events & POLLOUT)
		res |= POMP_FD_EVENT_OUT;
	if (events & POLLERR)
		res |= POMP_FD_EVENT_ERR;
	if (events & POLLHUP)
		res |= POMP_FD_EVENT_HUP;
	return res;
}

/**
 * Convert fd event to poll events.
 * @param events : fd events.
 * @return poll events.
 */
static int16_t fd_events_to_poll(uint32_t events)
{
	int16_t res = 0;
	if (events & POMP_FD_EVENT_IN)
		res |= POLLIN;
	if (events & POMP_FD_EVENT_PRI)
		res |= POLLPRI;
	if (events & POMP_FD_EVENT_OUT)
		res |= POLLOUT;
	if (events & POMP_FD_EVENT_ERR)
		res |= POLLERR;
	if (events & POMP_FD_EVENT_HUP)
		res |= POLLHUP;
	return res;
}

/**
 * Function called when the wakeup event is notified.
 * @param loop : loop object.
 */
static void pomp_loop_poll_wakeup_cb(struct pomp_loop *loop)
{
	/* Read from notification pipe */
	ssize_t res = 0;
	uint8_t dummy = 0;
	do {
		res = read(loop->wakeup.pipefds[0], &dummy, sizeof(dummy));
	} while (res < 0 && errno == EINTR);

	if (res < 0)
		POMP_LOG_FD_ERRNO("read", loop->wakeup.pipefds[0]);
}

/**
 * @see pomp_loop_do_new.
 */
static int pomp_loop_poll_do_new(struct pomp_loop *loop)
{
	int res = 0;

	/* Initialize implementation specific fields */
	loop->pollfds = NULL;
	loop->pollfdsize = 0;
	loop->wakeup.pipefds[0] = -1;
	loop->wakeup.pipefds[1] = -1;

	/* Create pipe for wakeup notification */
	if (pipe(loop->wakeup.pipefds) < 0) {
		res = -errno;
		POMP_LOG_ERRNO("pipe");
	}

	return res;
}

/**
 * @see pomp_loop_do_destroy.
 */
static int pomp_loop_poll_do_destroy(struct pomp_loop *loop)
{
	if (loop->wakeup.pipefds[0] >= 0) {
		close(loop->wakeup.pipefds[0]);
		loop->wakeup.pipefds[0] = -1;
	}

	if (loop->wakeup.pipefds[1] >= 0) {
		close(loop->wakeup.pipefds[1]);
		loop->wakeup.pipefds[1] = -1;
	}

	if (loop->pollfds != NULL) {
		free(loop->pollfds);
		loop->pollfds = NULL;
		loop->pollfdsize = 0;
	}
	return 0;
}

/**
 * @see pomp_loop_do_add.
 */
static int pomp_loop_poll_do_add(struct pomp_loop *loop, struct pomp_fd *pfd)
{
	/* Nothing to do */
	return 0;
}

/**
 * @see pomp_loop_do_update.
 */
static int pomp_loop_poll_do_update(struct pomp_loop *loop, struct pomp_fd *pfd)
{
	/* Nothing to do */
	return 0;
}

/**
 * @see pomp_loop_do_remove.
 */
static int pomp_loop_poll_do_remove(struct pomp_loop *loop, struct pomp_fd *pfd)
{
	/* Nothing to do */
	return 0;
}

/**
 * @see pomp_loop_do_get_fd.
 */
static intptr_t pomp_loop_poll_do_get_fd(struct pomp_loop *loop)
{
	return -ENOSYS;
}

/**
 * @see pomp_loop_do_wait_and_process.
 */
static int pomp_loop_poll_do_wait_and_process(struct pomp_loop *loop,
		int timeout)
{
	int res = 0;
	uint32_t i = 0, nevents = 0;
	struct pomp_fd *pfd = NULL;
	struct pollfd *pollfds = NULL;
	uint32_t revents = 0;
	uint32_t pfdcount = 0;

	/* Remember number of fds now because it can change during callback
	 * processing */
	pfdcount = loop->pfdcount + 1;

	/* Make sure internal pollfd array is big enough */
	if (loop->pollfdsize < pfdcount) {
		pollfds = realloc(loop->pollfds,
				pfdcount * sizeof(struct pollfd));
		if (pollfds == NULL)
			return -ENOMEM;
		loop->pollfds = pollfds;
		loop->pollfdsize = pfdcount;
	}
	memset(loop->pollfds, 0, loop->pollfdsize * sizeof(struct pollfd));

	/* Wakeup pipe */
	loop->pollfds[0].fd = loop->wakeup.pipefds[0];
	loop->pollfds[0].events = POLLIN;
	loop->pollfds[0].revents = 0;

	/* Registered fds */
	for (pfd = loop->pfds, i = 1; pfd != NULL; pfd = pfd->next, i++) {
		if (i >= pfdcount) {
			POMP_LOGE("Internal fd list corruption");
			break;
		}
		loop->pollfds[i].fd = pfd->fd;
		loop->pollfds[i].events = fd_events_to_poll(pfd->events);
		loop->pollfds[i].revents = 0;
	}

	/* Wait for poll events */
	do {
		res = poll(loop->pollfds, pfdcount, timeout);
	} while (res < 0 && errno == EINTR);

	if (res < 0) {
		res = -errno;
		POMP_LOG_ERRNO("poll");
		return res;
	}

	/* Process events */
	nevents = (uint32_t)res;
	for (i = 0; i < pfdcount; i++) {
		revents = fd_events_from_poll(loop->pollfds[i].revents);
		if (revents == 0)
			continue;

		/* Check for wakeup event */
		if (loop->pollfds[i].fd == loop->wakeup.pipefds[0]) {
			pomp_loop_poll_wakeup_cb(loop);
			continue;
		}

		/* The list might be modified during the callback call */
		pfd = pomp_loop_find_pfd(loop, loop->pollfds[i].fd);
		if (pfd != NULL)
			(*pfd->cb)(pfd->fd, revents, pfd->userdata);
	}

	return timeout == -1 ? 0 : (nevents > 0 ? 0 : -ETIMEDOUT);
}

/**
 * @see pomp_loop_do_wakeup.
 */
static int pomp_loop_poll_do_wakeup(struct pomp_loop *loop)
{
	ssize_t res = 0;
	uint8_t dummy = 1;

	/* Write to notification pipe */
	do {
		res = write(loop->wakeup.pipefds[1], &dummy, sizeof(dummy));
	} while (res < 0 && errno == EINTR);

	if (res < 0) {
		res = -errno;
		POMP_LOG_FD_ERRNO("write", loop->wakeup.pipefds[1]);
	} else {
		res = 0;
	}

	return res;
}

/** Loop operations for 'poll' implementation */
const struct pomp_loop_ops pomp_loop_poll_ops = {
	.do_new = &pomp_loop_poll_do_new,
	.do_destroy = &pomp_loop_poll_do_destroy,
	.do_add = &pomp_loop_poll_do_add,
	.do_update = &pomp_loop_poll_do_update,
	.do_remove = &pomp_loop_poll_do_remove,
	.do_get_fd = &pomp_loop_poll_do_get_fd,
	.do_wait_and_process = &pomp_loop_poll_do_wait_and_process,
	.do_wakeup = &pomp_loop_poll_do_wakeup,
};

#endif /* POMP_HAVE_LOOP_POLL */
