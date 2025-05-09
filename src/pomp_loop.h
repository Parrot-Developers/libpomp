/**
 * @file pomp_loop.h
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

#ifndef _POMP_LOOP_H_
#define _POMP_LOOP_H_

/** Check if current function is called from correct thread */
#define POMP_LOOP_CHECK_OWNER(loop) \
	pomp_loop_check_owner(loop, __func__)

/** Idle entry */
struct pomp_idle_entry {
	pomp_idle_cb_t		cb;		/**< Registered callback */
	void			*userdata;	/**< Callback user data */
	void			*cookie;	/**< Entry cookie */
	struct pomp_list_node	node;		/**< Entry in list */
};

/** Fd structure */
struct pomp_fd {
	intptr_t		fd;		/**< Associated fd (or ptr) */
	uint32_t		events;		/**< Monitored events */
	pomp_fd_event_cb_t	cb;		/**< Registered callback */
	void			*userdata;	/**< Callback user data */
	struct pomp_fd		*next;		/**< Next structure in list */

#ifdef POMP_HAVE_LOOP_WIN32
	int			nofd;		/**< 1 if fd is a fake fd */
	uint32_t		revents;	/**< Events ready */
#endif /* POMP_HAVE_LOOP_WIN32 */
};

/** Hash table length, number of bucket/slot */
#define POMP_LOOP_PFDS_LEN 128

/** Loop structure */
struct pomp_loop {
	/** Array of list of registered fds */
	struct pomp_fd		*pfds[POMP_LOOP_PFDS_LEN];
	uint32_t		pfdcount;	/**< Number of registered fds */

	struct pomp_list_node	idle_entries;	/**< Idle entries */
	struct pomp_evt		*idle_evt;	/**<  */
	int			is_destroying;	/**< Destruction Flag */

	pthread_mutex_t		lock;
	struct pomp_watchdog	watchdog;	/**< Watchdog */

	struct pomp_loop_sync	sync;		/**< Thread synchronization */

	struct {
		/* Thread that created the loop */
		pthread_t	creator;
		/* Thread currently doing a wait_and_process */
		pthread_t	waiter;
		/* Thread currently locking the loop */
		pthread_t	current;
	} owner;

	/** Owner checking enabled */
	int			owner_checked;


#ifdef POMP_HAVE_LOOP_POLL
	struct pollfd		*pollfds;	/**< Array of pollfd */
	uint32_t		pollfdsize;	/**< Allocate size of pollfds */
#endif /* POMP_HAVE_LOOP_POLL */

#ifdef POMP_HAVE_LOOP_EPOLL
	int			efd;		/**< epoll fd */
#endif /* POMP_HAVE_LOOP_EPOLL */

	/** Wakeup notification */
	struct {
#ifdef POMP_HAVE_LOOP_POLL
		int		pipefds[2];	/**< Pipes */
#endif /* POMP_HAVE_LOOP_POLL */

#ifdef POMP_HAVE_LOOP_EPOLL
		int		fd;		/**< event fd */
#endif /* POMP_HAVE_LOOP_EPOLL */

#ifdef POMP_HAVE_LOOP_WIN32
		HANDLE		hevt;		/**< Event handle */
#endif /* POMP_HAVE_LOOP_WIN32 */
	} wakeup;
};

/** Loop operations */
struct pomp_loop_ops {
	/** Implementation specific 'new' operation. */
	int (*do_new)(struct pomp_loop *loop);

	/** Implementation specific 'destroy' operation. */
	int (*do_destroy)(struct pomp_loop *loop);

	/** Implementation specific 'add' operation. */
	int (*do_add)(struct pomp_loop *loop, struct pomp_fd *pfd);

	/** Implementation specific 'update' operation. */
	int (*do_update)(struct pomp_loop *loop, struct pomp_fd *pfd);

	/** Implementation specific 'remove' operation. */
	int (*do_remove)(struct pomp_loop *loop, struct pomp_fd *pfd);

	/** Implementation specific 'get_fd' operation. */
	intptr_t (*do_get_fd)(struct pomp_loop *loop);

	/** Implementation specific 'wait_and_process' operation. */
	int (*do_wait_and_process)(struct pomp_loop *loop, int timeout);

	/** Implementation specific 'wakeup' operation. */
	int (*do_wakeup)(struct pomp_loop *loop);
};

/** Loop operations for 'poll' implementation */
#ifdef POMP_HAVE_LOOP_POLL
extern const struct pomp_loop_ops pomp_loop_poll_ops;
#endif /* POMP_HAVE_LOOP_POLL */

/** Loop operations for 'epoll' implementation */
#ifdef POMP_HAVE_LOOP_EPOLL
extern const struct pomp_loop_ops pomp_loop_epoll_ops;
#endif /* POMP_HAVE_LOOP_EPOLL */

/** Timer operations for 'win32' implementation */
#ifdef POMP_HAVE_LOOP_WIN32
extern const struct pomp_loop_ops pomp_loop_win32_ops;
#endif /* POMP_HAVE_LOOP_WIN32 */

/* Loop functions not part of public API */

const struct pomp_loop_ops *pomp_loop_set_ops(const struct pomp_loop_ops *ops);

struct pomp_fd *pomp_loop_find_pfd(struct pomp_loop *loop, intptr_t fd);

struct pomp_fd *pomp_loop_add_pfd(struct pomp_loop *loop, intptr_t fd,
		uint32_t events, pomp_fd_event_cb_t cb, void *userdata);

int pomp_loop_remove_pfd(struct pomp_loop *loop, struct pomp_fd *pfd);

void pomp_loop_check_owner(struct pomp_loop *loop, const char *caller);

#endif /* !_POMP_TIMER_H_ */
