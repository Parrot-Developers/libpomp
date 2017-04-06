/**
 * @file pomp_loop.c
 *
 * @brief Event loop implementation.
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

/* Include all available implementations */
#include "pomp_loop_linux.c"
#include "pomp_loop_posix.c"
#include "pomp_loop_win32.c"

/** Choose best implementation */
static const struct pomp_loop_ops *s_pomp_loop_ops =
#if defined(POMP_HAVE_LOOP_EPOLL)
	&pomp_loop_epoll_ops;
#elif defined(POMP_HAVE_LOOP_POLL)
	&pomp_loop_poll_ops;
#elif defined(POMP_HAVE_LOOP_WIN32)
	&pomp_loop_win32_ops;
#else
#error "No loop implementation available"
#endif

/**
 * For testing purposes, allow modification of loop operations.
 * @param ops : new loop operations.
 * @return previous loop operations.
 */
const struct pomp_loop_ops *pomp_loop_set_ops(const struct pomp_loop_ops *ops)
{
	const struct pomp_loop_ops *prev = s_pomp_loop_ops;
	s_pomp_loop_ops = ops;
	return prev;
}

/**
 * Implementation specific 'new' operation.
 * @param loop : loop.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int pomp_loop_do_new(struct pomp_loop *loop)
{
	return (*s_pomp_loop_ops->do_new)(loop);
}

/**
 * Implementation specific 'destroy' operation.
 * @param loop : loop.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int pomp_loop_do_destroy(struct pomp_loop *loop)
{
	return (*s_pomp_loop_ops->do_destroy)(loop);
}

/**
 * Implementation specific 'add' operation.
 * @param loop : loop.
 * @param pfd : fd structure.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int pomp_loop_do_add(struct pomp_loop *loop, struct pomp_fd *pfd)
{
	return (*s_pomp_loop_ops->do_add)(loop, pfd);
}

/**
 * Implementation specific 'update' operation.
 * @param loop : loop.
 * @param pfd : fd structure.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int pomp_loop_do_update(struct pomp_loop *loop, struct pomp_fd *pfd)
{
	return (*s_pomp_loop_ops->do_update)(loop, pfd);
}

/**
 * Implementation specific 'remove' operation.
 * @param loop : loop.
 * @param pfd : fd structure.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int pomp_loop_do_remove(struct pomp_loop *loop, struct pomp_fd *pfd)
{
	return (*s_pomp_loop_ops->do_remove)(loop, pfd);
}

/**
 * Implementation specific 'get_fd' operation.
 * @param loop : loop.
 * @return fd/event in case of success, negative errno value in case of error.
 */
static intptr_t pomp_loop_do_get_fd(struct pomp_loop *loop)
{
	return (*s_pomp_loop_ops->do_get_fd)(loop);
}

/**
 * Implementation specific 'wait_and_process' operation.
 * @param loop : loop.
 * @param timeout : timeout of wait (in ms) or -1 for infinite wait.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int pomp_loop_do_wait_and_process(struct pomp_loop *loop, int timeout)
{
	return (*s_pomp_loop_ops->do_wait_and_process)(loop, timeout);
}

/**
 * Implementation specific 'wakeup' operation.
 * @param loop : loop.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int pomp_loop_do_wakeup(struct pomp_loop *loop)
{
	return (*s_pomp_loop_ops->do_wakeup)(loop);
}

/**
 * Check if there is some idle entries to call.
 * @param loop : loop.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int pomp_loop_idle_check(struct pomp_loop *loop)
{
	uint32_t i = 0;
	struct pomp_idle_entry *entry = NULL;
	struct pomp_idle_entry *svg_idle_entries = NULL;
	uint32_t svg_idle_count = 0;
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);

	if (loop->idle_count == 0)
		return 0;

	/* keep ref on the current idle_entries */
	svg_idle_entries = loop->idle_entries;
	svg_idle_count = loop->idle_count;
	/* reset idle_entries to allow recursive idle entries */
	loop->idle_entries = NULL;
	loop->idle_count = 0;

	/* Call registered entries, preventing modification during the loop */
	for (i = 0; i < svg_idle_count; i++) {
		entry = &svg_idle_entries[i];
		if (!entry->removed)
			(*entry->cb)(entry->userdata);
	}

	/* Free old entries */
	free(svg_idle_entries);

	return 0;
}

/**
 * Find a registered fd in loop.
 * @param loop : loop.
 * @param fd : fd to search.
 * @return fd structure or NULL if not found.
 */
struct pomp_fd *pomp_loop_find_pfd(struct pomp_loop *loop, int fd)
{
	struct pomp_fd *pfd = NULL;
	for (pfd = loop->pfds; pfd != NULL; pfd = pfd->next) {
		if (pfd->fd == fd)
			return pfd;
	}
	return NULL;
}

/**
 * Register a new fd in loop.
 * @param loop : loop.
 * @param fd : fd to register.
 * @param events : events to monitor.
 * @param cb : callback for notifications.
 * @param userdata : user data for notifications.
 * @return fd structure or NULL in case of error.
 */
struct pomp_fd *pomp_loop_add_pfd(struct pomp_loop *loop, int fd,
		uint32_t events, pomp_fd_event_cb_t cb, void *userdata)
{
	struct pomp_fd *pfd = NULL;
	POMP_RETURN_VAL_IF_FAILED(loop != NULL, -EINVAL, NULL);

	/* Allocate our own structure */
	pfd = calloc(1, sizeof(*pfd));
	if (pfd == NULL)
		return NULL;

	/* Initialize structure */
	pfd->fd = fd;
	pfd->events = events;
	pfd->cb = cb;
	pfd->userdata = userdata;
	pfd->next = NULL;

	/* Add in our own list */
	pfd->next = loop->pfds;
	loop->pfds = pfd;
	loop->pfdcount++;

	return pfd;
}

/**
 * Remove a registered fd from loop.
 * @param loop : loop.
 * @param pfd : fd structure to remove (without destroying it).
 * @return 0 in case of success, negative errno value in case of error.
 */
int pomp_loop_remove_pfd(struct pomp_loop *loop, struct pomp_fd *pfd)
{
	struct pomp_fd *prev = NULL;
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(pfd != NULL, -EINVAL);

	if (loop->pfds == pfd) {
		/* This was the first in the list */
		loop->pfds = pfd->next;
		loop->pfdcount--;
		return 0;
	} else {
		for (prev = loop->pfds; prev != NULL; prev = prev->next) {
			if (prev->next == pfd) {
				/* Update link */
				prev->next = pfd->next;
				loop->pfdcount--;
				return 0;
			}
		}
	}
	POMP_LOGE("fd %d (%p) not found in loop %p", pfd->fd, pfd, loop);
	return -ENOENT;
}

/*
 * See documentation in public header.
 */
struct pomp_loop *pomp_loop_new(void)
{
	struct pomp_loop *loop = NULL;

	/* Allocate loop structure */
	loop = calloc(1, sizeof(*loop));
	if (loop == NULL)
		return NULL;

	/* Implementation specific */
	if (pomp_loop_do_new(loop) < 0) {
		free(loop);
		return NULL;
	}

	/* Success */
	return loop;
}

/*
 * See documentation in public header.
 */
int pomp_loop_destroy(struct pomp_loop *loop)
{
	int res = 0;
	struct pomp_fd *pfd = NULL;
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);

	/* Set destruction flag */
	loop->is_destroying = 1;

	/* Call idle entries */
	res = pomp_loop_idle_check(loop);
	if (res < 0)
		return res;

	if (loop->pfds) {
		for (pfd = loop->pfds; pfd != NULL; pfd = pfd->next) {
			POMP_LOGE("fd=%d, cb=%p not removed from loop",
					pfd->fd, pfd->cb);
		}
		return -EBUSY;
	}


	/* Implementation specific */
	res = pomp_loop_do_destroy(loop);
	if (res < 0)
		return res;

	/* Free resources */
	free(loop->idle_entries);
	free(loop);
	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_loop_add(struct pomp_loop *loop, int fd, uint32_t events,
		pomp_fd_event_cb_t cb, void *userdata)
{
	int res = 0;
	struct pomp_fd *pfd = NULL;
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(fd >= 0, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(events != 0, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(cb != NULL, -EINVAL);

	/* Make sure fd is not already registered */
	pfd = pomp_loop_find_pfd(loop, fd);
	if (pfd != NULL) {
		POMP_LOGW("fd %d (%p) already in loop %p", fd, pfd, loop);
		return -EEXIST;
	}

	/* Add our own structure */
	pfd = pomp_loop_add_pfd(loop, fd, events, cb, userdata);
	if (pfd == NULL)
		return -ENOMEM;

	/* Implementation specific */
	res = pomp_loop_do_add(loop, pfd);
	if (res < 0) {
		pomp_loop_remove_pfd(loop, pfd);
		free(pfd);
	}
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_loop_update(struct pomp_loop *loop, int fd, uint32_t events)
{
	int res = 0;
	struct pomp_fd *pfd = NULL;
	uint32_t oldevents = 0;
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(fd >= 0, -EINVAL);

	/* Make sure fd is registered */
	pfd = pomp_loop_find_pfd(loop, fd);
	if (pfd == NULL) {
		POMP_LOGW("fd %d not found in loop %p", fd, loop);
		return -ENOENT;
	}

	/* Implementation specific */
	oldevents = pfd->events;
	pfd->events = events;
	res = pomp_loop_do_update(loop, pfd);
	if (res < 0)
		pfd->events = oldevents;
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_loop_update2(struct pomp_loop *loop, int fd,
		uint32_t events_to_add, uint32_t events_to_remove)
{
	int res = 0;
	struct pomp_fd *pfd = NULL;
	uint32_t oldevents = 0;
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(fd >= 0, -EINVAL);

	/* Make sure fd is registered */
	pfd = pomp_loop_find_pfd(loop, fd);
	if (pfd == NULL) {
		POMP_LOGW("fd %d not found in loop %p", fd, loop);
		return -ENOENT;
	}

	/* Implementation specific */
	oldevents = pfd->events;
	pfd->events |= events_to_add;
	pfd->events &= ~events_to_remove;
	res = pomp_loop_do_update(loop, pfd);
	if (res < 0)
		pfd->events = oldevents;
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_loop_remove(struct pomp_loop *loop, int fd)
{
	int res = 0;
	struct pomp_fd *pfd = NULL;
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(fd >= 0, -EINVAL);

	/* Make sure fd is registered */
	pfd = pomp_loop_find_pfd(loop, fd);
	if (pfd == NULL) {
		POMP_LOGW("fd %d not found in loop %p", fd, loop);
		return -ENOENT;
	}

	/* Implementation specific */
	pomp_loop_do_remove(loop, pfd);

	/* Always remove from our own list */
	res = pomp_loop_remove_pfd(loop, pfd);
	if (res == 0)
		free(pfd);
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_loop_has_fd(struct pomp_loop *loop, int fd)
{
	return (loop && pomp_loop_find_pfd(loop, fd)) ? 1 : 0;
}

/*
 * See documentation in public header.
 */
intptr_t pomp_loop_get_fd(struct pomp_loop *loop)
{
	/* Implementation specific */
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);
	return pomp_loop_do_get_fd(loop);
}

/*
 * See documentation in public header.
 */
int pomp_loop_process_fd(struct pomp_loop *loop)
{
	return pomp_loop_wait_and_process(loop, 0);
}

/*
 * See documentation in public header.
 */
int pomp_loop_wait_and_process(struct pomp_loop *loop, int timeout)
{
	int res = 0;
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);

	/* Implementation specific */
	res = pomp_loop_do_wait_and_process(loop, timeout);

	/* Check for idle function to call */
	pomp_loop_idle_check(loop);

	return res;
}

/*
 * See documentation in public header.
 */
int pomp_loop_wakeup(struct pomp_loop *loop)
{
	/* Implementation specific */
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);
	return pomp_loop_do_wakeup(loop);
}

/*
 * See documentation in public header.
 */
int pomp_loop_idle_add(struct pomp_loop *loop, pomp_idle_cb_t cb,
		void *userdata)
{
	struct pomp_idle_entry *newentries = NULL;
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(cb != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(!loop->is_destroying, -EPERM);

	/* Allocate entry */
	newentries = realloc(loop->idle_entries, (loop->idle_count + 1)
			 * sizeof(struct pomp_idle_entry));
	if (newentries == NULL)
		return -ENOMEM;
	loop->idle_entries = newentries;

	/* Force loop wake up */
	if (loop->idle_count == 0)
		pomp_loop_wakeup(loop);

	/* Save entry */
	loop->idle_entries[loop->idle_count].cb = cb;
	loop->idle_entries[loop->idle_count].userdata = userdata;
	loop->idle_entries[loop->idle_count].removed = 0;
	loop->idle_count++;

	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_loop_idle_remove(struct pomp_loop *loop, pomp_idle_cb_t cb,
		void *userdata)
{
	uint32_t i = 0;
	struct pomp_idle_entry *entry = NULL;
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);

	/* Walk entries, just mark matching entries as removed, next call to
	 * pomp_loop_idle_check will do the cleaning */
	for (i = 0; i < loop->idle_count; i++) {
		entry = &loop->idle_entries[i];
		if (entry->cb == cb && entry->userdata == userdata)
			entry->removed = 1;
	}

	return 0;
}
