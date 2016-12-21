/**
 * @file pomp_loop_linux.c
 *
 * @brief Event loop, linux 'epoll' implementation.
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

#ifdef POMP_HAVE_LOOP_EPOLL

/** epoll event structure initializer */
#ifndef EPOLL_EVENT_INITIALIZER
#  define EPOLL_EVENT_INITIALIZER	{0, {0} }
#endif /* !EPOLL_EVENT_INITIALIZER */

/**
 * Convert fd event from epoll events.
 * @param events : epoll events.
 * @return fd events.
 */
static uint32_t fd_events_from_epoll(uint32_t events)
{
	uint32_t res = 0;
	if (events & EPOLLIN)
		res |= POMP_FD_EVENT_IN;
	if (events & EPOLLPRI)
		res |= POMP_FD_EVENT_PRI;
	if (events & EPOLLOUT)
		res |= POMP_FD_EVENT_OUT;
	if (events & EPOLLERR)
		res |= POMP_FD_EVENT_ERR;
	if (events & EPOLLHUP)
		res |= POMP_FD_EVENT_HUP;
	return res;
}

/**
 * Convert fd event to epoll events.
 * @param events : fd events.
 * @return epoll events.
 */
static uint32_t fd_events_to_epoll(uint32_t events)
{
	uint32_t res = 0;
	if (events & POMP_FD_EVENT_IN)
		res |= EPOLLIN;
	if (events & POMP_FD_EVENT_PRI)
		res |= EPOLLPRI;
	if (events & POMP_FD_EVENT_OUT)
		res |= EPOLLOUT;
	if (events & POMP_FD_EVENT_ERR)
		res |= EPOLLERR;
	if (events & POMP_FD_EVENT_HUP)
		res |= EPOLLHUP;
	return res;
}

/**
 * Execute an epoll operation on a registered fd.
 * @param loop : loop.
 * @param op : operation to execute.
 * @param pfd : fd structure.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int pomp_loop_epoll_ctl(struct pomp_loop *loop, int op,
		struct pomp_fd *pfd)
{
	int res = 0;
	struct epoll_event event = EPOLL_EVENT_INITIALIZER;

	event.events = fd_events_to_epoll(pfd->events);
	event.data.fd = pfd->fd;
	if (epoll_ctl(loop->efd, op, pfd->fd, &event) < 0) {
		res = -errno;
		POMP_LOG_ERRNO("epoll_ctl");
	}
	return res;
}

/**
 * Function called when the wakeup event is notified.
 * @param loop : loop.
 */
static void pomp_loop_epoll_wakeup_cb(struct pomp_loop *loop)
{
	/* Read from event fd */
	ssize_t res = 0;
	uint64_t value = 0;
	do {
		res = read(loop->wakeup.fd, &value, sizeof(value));
	} while (res < 0 && errno == EINTR);

	if (res < 0)
		POMP_LOG_FD_ERRNO("read", loop->wakeup.fd);
}

/**
 * @see pomp_loop_do_new.
 */
static int pomp_loop_epoll_do_new(struct pomp_loop *loop)
{
	int res = 0;
	struct epoll_event event = EPOLL_EVENT_INITIALIZER;

	/* Initialize implementation specific fields */
	loop->efd = -1;
	loop->wakeup.fd = -1;

	/* Create epoll fd */
	loop->efd = epoll_create(1);
	if (loop->efd < 0) {
		res = -errno;
		POMP_LOG_ERRNO("epoll_create");
		goto error;
	}

	/* Setup fd flags */
	res = fd_setup_flags(loop->efd);
	if (res < 0)
		goto error;

	/* Create event fd for notification */
	loop->wakeup.fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (loop->wakeup.fd < 0) {
		res = -errno;
		POMP_LOG_ERRNO("eventfd");
		goto error;
	}

	/* Monitor it */
	event.events = EPOLLIN;
	event.data.fd = loop->wakeup.fd;
	if (epoll_ctl(loop->efd, EPOLL_CTL_ADD, loop->wakeup.fd, &event) < 0) {
		res = -errno;
		POMP_LOG_ERRNO("epoll_ctl");
		goto error;
	}

	return 0;

	/* Cleanup in case of error */
error:
	if (loop->wakeup.fd >= 0) {
		close(loop->wakeup.fd);
		loop->wakeup.fd = -1;
	}

	if (loop->efd >= 0) {
		close(loop->efd);
		loop->efd = -1;
	}
	return res;
}

/**
 * @see pomp_loop_do_destroy.
 */
static int pomp_loop_epoll_do_destroy(struct pomp_loop *loop)
{
	/* Free resources */
	if (loop->efd >= 0) {
		if (loop->wakeup.fd >= 0) {
			epoll_ctl(loop->efd, EPOLL_CTL_DEL,
					loop->wakeup.fd, NULL);
			close(loop->wakeup.fd);
			loop->wakeup.fd = -1;
		}

		close(loop->efd);
		loop->efd = -1;
	}

	return 0;
}

/**
 * @see pomp_loop_do_add.
 */
static int pomp_loop_epoll_do_add(struct pomp_loop *loop, struct pomp_fd *pfd)
{
	return pomp_loop_epoll_ctl(loop, EPOLL_CTL_ADD, pfd);
}

/**
 * @see pomp_loop_do_update.
 */
static int pomp_loop_epoll_do_update(struct pomp_loop *loop,
		struct pomp_fd *pfd)
{
	return pomp_loop_epoll_ctl(loop, EPOLL_CTL_MOD, pfd);
}

/**
 * @see pomp_loop_do_remove.
 */
static int pomp_loop_epoll_do_remove(struct pomp_loop *loop,
		struct pomp_fd *pfd)
{
	return pomp_loop_epoll_ctl(loop, EPOLL_CTL_DEL, pfd);
}

/**
 * @see pomp_loop_do_get_fd.
 */
static intptr_t pomp_loop_epoll_do_get_fd(struct pomp_loop *loop)
{
	return loop->efd;
}

/**
 * @see pomp_loop_do_wait_and_process.
 */
static int pomp_loop_epoll_do_wait_and_process(struct pomp_loop *loop,
		int timeout)
{
	int res = 0;
	uint32_t i = 0, nevents = 0;
	struct epoll_event events[16];
	struct pomp_fd *pfd = NULL;
	uint32_t revents = 0;

	/* Wait for epoll events */
	do {
		nevents = sizeof(events) / sizeof(events[0]);
		res = epoll_wait(loop->efd, events, (int)nevents, timeout);
	} while (res < 0 && errno == EINTR);

	if (res < 0) {
		res = -errno;
		POMP_LOG_ERRNO("epoll_wait");
		return res;
	}

	/* Process events */
	nevents = (uint32_t)res;
	for (i = 0; i < nevents; i++) {
		revents = fd_events_from_epoll(events[i].events);
		if (revents == 0)
			continue;

		/* Check for wakeup event */
		if (events[i].data.fd == loop->wakeup.fd) {
			pomp_loop_epoll_wakeup_cb(loop);
			continue;
		}

		/* The list might be modified during the callback call */
		pfd = pomp_loop_find_pfd(loop, events[i].data.fd);
		if (pfd != NULL)
			(*pfd->cb)(pfd->fd, revents, pfd->userdata);
	}

	return timeout == -1 ? 0 : (nevents > 0 ? 0 : -ETIMEDOUT);
}

/**
 * @see pomp_loop_do_wakeup.
 */
static int pomp_loop_epoll_do_wakeup(struct pomp_loop *loop)
{
	/* Write to event fd */
	ssize_t res = 0;
	uint64_t value = 1;
	do {
		res = write(loop->wakeup.fd, &value, sizeof(value));
	} while (res < 0 && errno == EINTR);

	if (res < 0) {
		res = -errno;
		POMP_LOG_FD_ERRNO("write", loop->wakeup.fd);
	} else {
		res = 0;
	}

	return res;
}

/** Loop operations for linux 'epoll' implementation */
const struct pomp_loop_ops pomp_loop_epoll_ops = {
	.do_new = &pomp_loop_epoll_do_new,
	.do_destroy = &pomp_loop_epoll_do_destroy,
	.do_add = &pomp_loop_epoll_do_add,
	.do_update = &pomp_loop_epoll_do_update,
	.do_remove = &pomp_loop_epoll_do_remove,
	.do_get_fd = &pomp_loop_epoll_do_get_fd,
	.do_wait_and_process = &pomp_loop_epoll_do_wait_and_process,
	.do_wakeup = &pomp_loop_epoll_do_wakeup,
};

#endif /* POMP_HAVE_LOOP_EPOLL */
