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
 * Return hash table index for given file descriptor
 * @param fd : fd to hash
 * @return an index in loop->pfds[]
 */
static inline unsigned int pomp_loop_index_fd(intptr_t fd)
{
	uintptr_t x = fd;

	x *= UINT32_C(0xefec2401);
	x ^= x >> 4;

	return x % POMP_LOOP_PFDS_LEN;
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
	unsigned int pfdi = pomp_loop_index_fd(fd);

	for (pfd = loop->pfds[pfdi]; pfd != NULL; pfd = pfd->next) {
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
	unsigned int pfdi;
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
	pfdi = pomp_loop_index_fd(fd);
	pfd->next = loop->pfds[pfdi];
	loop->pfds[pfdi] = pfd;
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
	unsigned int pfdi;
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(pfd != NULL, -EINVAL);

	pfdi = pomp_loop_index_fd(pfd->fd);

	if (loop->pfds[pfdi] == pfd) {
		/* This was the first in the list */
		loop->pfds[pfdi] = pfd->next;
		loop->pfdcount--;
		return 0;
	} else {
		for (prev = loop->pfds[pfdi]; prev != NULL; prev = prev->next) {
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
	struct pomp_list_node *node = NULL;
	struct pomp_idle_entry *entry = NULL;
	POMP_RETURN_IF_FAILED(loop != NULL, -EINVAL);

	pthread_mutex_lock(&loop->lock);
	if (pomp_list_is_empty(&loop->idle_entries)) {
		pthread_mutex_unlock(&loop->lock);
		return;
	}

	/* Remove the first entry */
	node = pomp_list_first(&loop->idle_entries);
	entry = pomp_list_entry(node, struct pomp_idle_entry, node);
	pomp_list_remove(node);

	/* Call callback outside lock */
	pthread_mutex_unlock(&loop->lock);
	(*entry->cb)(entry->userdata);
	free(entry);
	pthread_mutex_lock(&loop->lock);

	if (!pomp_list_is_empty(&loop->idle_entries))
		pomp_evt_signal(loop->idle_evt);

	pthread_mutex_unlock(&loop->lock);
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

	pomp_watchdog_init(&loop->watchdog);
	pthread_mutex_init(&loop->lock, NULL);
	pomp_list_init(&loop->idle_entries);
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
	struct pomp_list_node *node = NULL;
	struct pomp_idle_entry *entry = NULL;
	struct pomp_fd *pfd = NULL;
	unsigned int pfdi;
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);

	/* Detach the event for idle entries */
	if (!loop->is_destroying)
		pomp_evt_detach_from_loop(loop->idle_evt, loop);

	/* Set destruction flag */
	loop->is_destroying = 1;

	/* Check for remaining idle entries. calling flush here is not safe
	 * associated callbacks and userdata may have been destroyed already
	 * pomp_loop_idle_flush should be called explicitely before when safe.
	 */
	pomp_list_walk_forward(&loop->idle_entries, node) {
		entry = pomp_list_entry(node, struct pomp_idle_entry, node);
		POMP_LOGE("idle entry cb=%p userdata=%p still in the loop",
				entry->cb, entry->userdata);
		res = -EBUSY;
	}

	for (pfdi = 0; pfdi < POMP_LOOP_PFDS_LEN; pfdi++) {
		for (pfd = loop->pfds[pfdi]; pfd != NULL; pfd = pfd->next) {
			POMP_LOGE("fd=%" PRIiPTR ", cb=%p still in loop",
					pfd->fd, pfd->cb);
			res = -EBUSY;
		}
	}

	if (res < 0)
		return res;

	pthread_mutex_destroy(&loop->lock);
	pomp_watchdog_clear(&loop->watchdog);

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
	struct pomp_idle_entry *entry = NULL;
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(cb != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(!loop->is_destroying, -EPERM);

	/* Allocate entry */
	entry = calloc(1, sizeof(*entry));
	if (entry == NULL)
		return -ENOMEM;
	/* Initialize entry */
	entry->cb = cb;
	entry->userdata = userdata;
	entry->cookie = NULL;

	pthread_mutex_lock(&loop->lock);

	/* Put at the back of the list */
	pomp_list_add_tail(&loop->idle_entries, &entry->node);

	pthread_mutex_unlock(&loop->lock);

	/* Force loop wake up */
	res = pomp_evt_signal(loop->idle_evt);
	if (res < 0)
		POMP_LOGE("pomp_evt_signal err=%d(%s)", -res, strerror(-res));

	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_loop_idle_add_with_cookie(struct pomp_loop *loop, pomp_idle_cb_t cb,
		void *userdata, void *cookie)
{
	int res;
	struct pomp_idle_entry *entry = NULL;
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(cb != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(cookie != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(!loop->is_destroying, -EPERM);

	/* Allocate entry */
	entry = calloc(1, sizeof(*entry));
	if (entry == NULL)
		return -ENOMEM;
	/* Initialize entry */
	entry->cb = cb;
	entry->userdata = userdata;
	entry->cookie = cookie;

	pthread_mutex_lock(&loop->lock);

	/* Put at the back of the list */
	pomp_list_add_tail(&loop->idle_entries, &entry->node);

	pthread_mutex_unlock(&loop->lock);

	/* Force loop wake up */
	res = pomp_evt_signal(loop->idle_evt);
	if (res < 0)
		POMP_LOGE("pomp_evt_signal err=%d(%s)", -res, strerror(-res));

	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_loop_idle_remove(struct pomp_loop *loop, pomp_idle_cb_t cb,
		void *userdata)
{
	struct pomp_list_node *node = NULL, *tmp = NULL;
	struct pomp_idle_entry *entry = NULL;
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);

	pthread_mutex_lock(&loop->lock);

	/* Walk entries to remove all corresponding ones. */
	pomp_list_walk_forward_safe(&loop->idle_entries, node, tmp) {
		entry = pomp_list_entry(node, struct pomp_idle_entry, node);
		if (entry->cb == cb && entry->userdata == userdata) {
			pomp_list_remove(node);
			free(entry);
		}
	}

	if (pomp_list_is_empty(&loop->idle_entries))
		pomp_evt_clear(loop->idle_evt);

	pthread_mutex_unlock(&loop->lock);

	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_loop_idle_remove_by_cookie(struct pomp_loop *loop, void *cookie)
{
	struct pomp_list_node *node = NULL, *tmp = NULL;
	struct pomp_idle_entry *entry = NULL;
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(cookie != NULL, -EINVAL);

	pthread_mutex_lock(&loop->lock);

	/* Walk entries to remove all corresponding ones. */
	pomp_list_walk_forward_safe(&loop->idle_entries, node, tmp) {
		entry = pomp_list_entry(node, struct pomp_idle_entry, node);
		if (entry->cookie == cookie) {
			pomp_list_remove(node);
			free(entry);
		}
	}

	if (pomp_list_is_empty(&loop->idle_entries))
		pomp_evt_clear(loop->idle_evt);

	pthread_mutex_unlock(&loop->lock);

	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_loop_idle_flush(struct pomp_loop *loop)
{
	struct pomp_list_node *node = NULL;
	struct pomp_idle_entry *entry = NULL;
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);

	pthread_mutex_lock(&loop->lock);

	/* While there are still entries in the idle-list */
	while (!pomp_list_is_empty(&loop->idle_entries)) {
		/* Remove the first entry */
		node = pomp_list_first(&loop->idle_entries);
		entry = pomp_list_entry(node, struct pomp_idle_entry, node);
		pomp_list_remove(node);

		/* Call callback outside lock */
		pthread_mutex_unlock(&loop->lock);
		(*entry->cb)(entry->userdata);
		free(entry);
		pthread_mutex_lock(&loop->lock);
	}

	pomp_evt_clear(loop->idle_evt);

	pthread_mutex_unlock(&loop->lock);

	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_loop_idle_flush_by_cookie(struct pomp_loop *loop,
		void *cookie)
{
	struct pomp_list_node *node = NULL, *tmp = NULL;
	struct pomp_idle_entry *entry = NULL;
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(cookie != NULL, -EINVAL);

	pthread_mutex_lock(&loop->lock);

	/* Walk entries to remove all corresponding ones. */
again:
	pomp_list_walk_forward_safe(&loop->idle_entries, node, tmp) {
		entry = pomp_list_entry(node, struct pomp_idle_entry, node);
		if (entry->cookie == cookie) {
			pomp_list_remove(node);

			/* Call callback outside lock */
			pthread_mutex_unlock(&loop->lock);
			(*entry->cb)(entry->userdata);
			free(entry);
			pthread_mutex_lock(&loop->lock);

			/* As soon as a callback is called outside lock, we need
			 * to start again the walk from the start in case the
			 * list was modified */
			goto again;
		}
	}

	if (pomp_list_is_empty(&loop->idle_entries))
		pomp_evt_clear(loop->idle_evt);

	pthread_mutex_unlock(&loop->lock);

	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_loop_watchdog_enable(struct pomp_loop *loop,
		uint32_t delay,
		pomp_watchdog_cb_t cb,
		void *userdata)
{
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(delay > 0, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(cb != NULL, -EINVAL);
	return pomp_watchdog_start(&loop->watchdog, loop, delay, cb, userdata);
}

/*
 * See documentation in public header.
 */
int pomp_loop_watchdog_disable(struct pomp_loop *loop)
{
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);
	return pomp_watchdog_stop(&loop->watchdog);
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
