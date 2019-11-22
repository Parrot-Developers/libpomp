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
 * Flush idle entries.
 * Calls all pending idles.
 *
 * @param loop : loop.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int pomp_loop_idle_flush(struct pomp_loop *loop)
{
	struct pomp_idle_entry *entry;
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);

	/* While there are still entries in the idle-list */
	while (loop->idle_entries != NULL) {
		/* Take the first element */
		entry = loop->idle_entries->next;
		/* Was it also the last element? */
		if (entry == loop->idle_entries)
			loop->idle_entries = NULL;
		else
			loop->idle_entries->next = entry->next;
		(*entry->cb)(entry->userdata);
		free(entry);
	/* If entry was the last element, stop the loop */
	}

	return 0;
}

/**
 * Find a registered fd in loop.
 * @param loop : loop.
 * @param fd : fd to search.
 * @return fd structure or NULL if not found.
 */
struct pomp_fd *pomp_loop_find_pfd(struct pomp_loop *loop, intptr_t fd)
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
struct pomp_fd *pomp_loop_add_pfd(struct pomp_loop *loop, intptr_t fd,
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
	POMP_LOGE("fd %" PRIiPTR " (%p) not found in loop %p",
			pfd->fd, pfd, loop);
	return -ENOENT;
}

static void pomp_idle_evt_cb(struct pomp_evt *evt, void *userdata)
{
	struct pomp_loop *loop = userdata;
	struct pomp_idle_entry *entry;
	POMP_RETURN_IF_FAILED(loop != NULL, -EINVAL);

	if (loop->idle_entries == NULL)
		return;

	/* Point to first entry */
	entry = loop->idle_entries->next;

	/* Remove the first entry */
	if (entry == loop->idle_entries)
		loop->idle_entries = NULL;
	else
		loop->idle_entries->next = entry->next;

	(*entry->cb)(entry->userdata);

	free(entry);

	if (loop->idle_entries != NULL)
		pomp_evt_signal(loop->idle_evt);
}

/*
 * See documentation in public header.
 */
struct pomp_loop *pomp_loop_new(void)
{
	int res;
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

	loop->idle_entries = NULL;
	loop->idle_evt = pomp_evt_new();
	if (loop->idle_evt == NULL)
		goto error;

	res = pomp_evt_attach_to_loop(loop->idle_evt, loop,
			&pomp_idle_evt_cb, loop);
	if (res < 0)
		goto error;

	/* Success */
	return loop;
error:
	pomp_loop_destroy(loop);
	return NULL;
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
	res = pomp_loop_idle_flush(loop);
	if (res < 0)
		return res;

	/* Detach the event */
	pomp_evt_detach_from_loop(loop->idle_evt, loop);

	if (loop->pfds) {
		for (pfd = loop->pfds; pfd != NULL; pfd = pfd->next) {
			POMP_LOGE("fd=%" PRIiPTR ", cb=%p still in loop",
					pfd->fd, pfd->cb);
		}
		return -EBUSY;
	}

	/* Implementation specific */
	res = pomp_loop_do_destroy(loop);
	if (res < 0)
		return res;

	/* Free resources */
	pomp_evt_destroy(loop->idle_evt);
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
	int res;
	struct pomp_idle_entry *newentry = NULL, *rear_entry = NULL;
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(cb != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(!loop->is_destroying, -EPERM);

	/* Allocate entry */
	newentry = calloc(1, sizeof(*newentry));
	if (newentry == NULL)
		return -ENOMEM;
	/* Initialize entry */
	newentry->cb = cb;
	newentry->userdata = userdata;

	/* Put at the back of the list */
	rear_entry = loop->idle_entries;
	loop->idle_entries = newentry;
	/* Make sure next pointer of the last entry targets the first one */
	if (rear_entry == NULL) {
		newentry->next = newentry;
	} else {
		newentry->next = rear_entry->next;
		rear_entry->next = newentry;
	}

	/* Force loop wake up */
	res = pomp_evt_signal(loop->idle_evt);
	if (res < 0) {
		POMP_LOGE("pomp_evt_signal err=%d(%s)", -res, strerror(-res));
		goto error;
	}

	return 0;
error:
	/* Restore previous rear entry */
	loop->idle_entries = rear_entry;
	if (rear_entry != NULL)
		rear_entry->next = newentry->next;
	free(newentry);
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_loop_idle_remove(struct pomp_loop *loop, pomp_idle_cb_t cb,
		void *userdata)
{
	struct pomp_idle_entry *entry = NULL, *prev_entry = NULL;
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);

	/* Walk entries to remove all corresponding ones. */
	prev_entry = loop->idle_entries;
	while (loop->idle_entries != entry) {
		entry = prev_entry->next;
		if (entry->cb == cb && entry->userdata == userdata) {
			/* Unlink entry from list */
			prev_entry->next = entry->next;
			free(entry);
			/* Was it the last entry? */
			if (loop->idle_entries == entry) {
				/* Only entry remaining in the list? */
				if (prev_entry == entry)
					loop->idle_entries = entry = NULL;
				else
					loop->idle_entries = entry = prev_entry;
			}
		} else {
			prev_entry = entry;
		}
	}

	if (loop->idle_entries == NULL)
		pomp_evt_clear(loop->idle_evt);

	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_internal_set_loop_impl(enum pomp_loop_impl impl)
{
	switch (impl) {
	case POMP_LOOP_IMPL_EPOLL:
#ifdef POMP_HAVE_LOOP_EPOLL
		pomp_loop_set_ops(&pomp_loop_epoll_ops);
		return 0;
#else
		return -EINVAL;
#endif

	case POMP_LOOP_IMPL_POLL:
#ifdef POMP_HAVE_LOOP_POLL
		pomp_loop_set_ops(&pomp_loop_poll_ops);
		return 0;
#else
		return -EINVAL;
#endif

	case POMP_LOOP_IMPL_WIN32:
#ifdef POMP_HAVE_LOOP_WIN32
		pomp_loop_set_ops(&pomp_loop_win32_ops);
		return 0;
#else
		return -EINVAL;
#endif

	default:
		return -EINVAL;
	}
}
