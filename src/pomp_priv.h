/**
 * @file pomp_priv.h
 *
 * @brief private headers.
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

#ifndef _POMP_PRIV_H_
#define _POMP_PRIV_H_

#define _GNU_SOURCE

/* Generic headers */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <inttypes.h>

#ifndef _MSC_VER
#  include <unistd.h>
#else /* !_MSC_VER */
#  include <io.h>
#endif /* !_MSC_VER */

/* Detect available system headers */
#include "pomp_config.h"

/* Include system specific headers */
#ifdef HAVE_NETDB_H
#  include <netdb.h>
#endif
#ifdef HAVE_SYS_EPOLL_H
#  include <sys/epoll.h>
#  define POMP_HAVE_LOOP_EPOLL
#endif
#ifdef HAVE_SYS_EVENT_H
#  include <sys/event.h>
#  define POMP_HAVE_TIMER_KQUEUE
#endif
#ifdef HAVE_SYS_EVENTFD_H
#  include <sys/eventfd.h>
#elif defined(__linux__)
#  include "sys_eventfd.h"
#endif
#ifdef HAVE_SYS_PARAM_H
#  include <sys/param.h>
#endif
#ifdef HAVE_SYS_POLL_H
#  include <sys/poll.h>
#  define POMP_HAVE_LOOP_POLL
#endif
#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif
#ifdef HAVE_SYS_TIMERFD_H
#  include <sys/timerfd.h>
#  define POMP_HAVE_TIMER_FD
#elif defined(__linux__)
#  include "sys_timerfd.h"
#  define POMP_HAVE_TIMER_FD
#endif
#ifdef HAVE_SYS_UN_H
#  include <sys/un.h>
#endif
#ifdef HAVE_NETINET_TCP_H
#  include <netinet/tcp.h>
#endif

/* Detect available implementations */
#if !defined(POMP_HAVE_TIMER_POSIX) && defined(HAVE_TIMER_CREATE)
#  define POMP_HAVE_TIMER_POSIX
#endif

#ifdef _WIN32
#  include "pomp_priv_win32.h"
#endif /* _WIN32 */

#if defined(__FreeBSD__) || defined(__APPLE__)
#  include "pomp_priv_bsd.h"
#endif /* __FreeBSD__ || __APPLE__ */

/** Signal number to use for posix timer implementation */
#ifndef POMP_TIMER_POSIX_SIGNO
#  define POMP_TIMER_POSIX_SIGNO	SIGRTMIN
#endif /* POMP_TIMER_POSIX_SIGNO */

#include "libpomp.h"

#include "pomp_log.h"
#include "pomp_buffer.h"
#include "pomp_timer.h"
#include "pomp_loop.h"
#include "pomp_prot.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Message structure initializer */
#define POMP_MSG_INITIALIZER		{0, 0, NULL}

/** Encoder structure initializer*/
#define POMP_ENCODER_INITIALIZER	{NULL, 0}

/** Decoder structure initializer*/
#define POMP_DECODER_INITIALIZER	{NULL, 0}

/** Message data */
struct pomp_msg {
	uint32_t		msgid;		/**< Id of message */
	uint32_t		finished;	/**< Header is filled */
	struct pomp_buffer	*buf;		/**< Buffer with data */
};

/** Encode state */
struct pomp_encoder {
	struct pomp_msg		*msg;		/**< Associated message */
	size_t			pos;		/**< Position in data */
};

/** Decoder state */
struct pomp_decoder {
	const struct pomp_msg	*msg;		/**< Associated message */
	size_t			pos;		/**< Position in data */
};

/** Value union */
union pomp_value {
	int8_t			i8;		/**< i8 value */
	uint8_t			u8;		/**< u8 value */
	int16_t			i16;		/**< i16 value */
	uint16_t		u16;		/**< u16 value */
	int32_t			i32;		/**< i32 value */
	uint32_t		u32;		/**< u32 value */
	int64_t			i64;		/**< i64 value */
	uint64_t		u64;		/**< u64 value */
	char			*str;		/**< str value */
	const char		*cstr;		/**< cstr value */
	void			*buf;		/**< buf value */
	const void		*cbuf;		/**< cbuf value */
	float			f32;		/**< f32 value */
	double			f64;		/**< f64 value */
	int			fd;		/**< fd value */
};

/* Context functions not part of public API and called from connection */

int pomp_ctx_remove_conn(struct pomp_ctx *ctx, struct pomp_conn *conn);

int pomp_ctx_notify_event(struct pomp_ctx *ctx, enum pomp_event event,
		struct pomp_conn *conn);

int pomp_ctx_notify_msg(struct pomp_ctx *ctx, struct pomp_conn *conn,
		const struct pomp_msg *msg);

int pomp_ctx_notify_raw_buf(struct pomp_ctx *ctx, struct pomp_conn *conn,
		struct pomp_buffer *buf);

int pomp_ctx_notify_send(struct pomp_ctx *ctx, struct pomp_conn *conn,
		struct pomp_buffer *buf, uint32_t status);

int pomp_ctx_sendcb_is_set(struct pomp_ctx *ctx);

/* Connection functions not part of public API */

struct pomp_conn *pomp_conn_new(struct pomp_ctx *ctx,
		struct pomp_loop *loop, int fd, int isdgram, int israw);

int pomp_conn_destroy(struct pomp_conn *conn);

int pomp_conn_close(struct pomp_conn *conn);

struct pomp_conn *pomp_conn_get_next(const struct pomp_conn *conn);

int pomp_conn_set_next(struct pomp_conn *conn, struct pomp_conn *next);

int pomp_conn_send_msg_to(struct pomp_conn *conn,
		const struct pomp_msg *msg,
		const struct sockaddr *addr, uint32_t addrlen);

int pomp_conn_send_raw_buf_to(struct pomp_conn *conn,
		struct pomp_buffer *buf,
		const struct sockaddr *addr, uint32_t addrlen);

/* Decoder functions not part of public API */

/**
 * Decoder dump callback.
 * @param dec : decoder.
 * @param type : type of argument.
 * @param v : value of argument.
 * @param buflen : buffer length for buffer argument.
 * @param userdata : callback user data.
 * @return 1 to continue walk, 0 to stop it.
 */
typedef int (*pomp_decoder_walk_cb_t)(struct pomp_decoder *dec, uint8_t type,
		const union pomp_value *v, uint32_t buflen, void *userdata);

int pomp_decoder_walk(struct pomp_decoder *dec,
		pomp_decoder_walk_cb_t cb, void *userdata,
		int checkfds);

/* Fd utilities */

/**
 * Set the FD_CLOEXEC flag of fd.
 * @param fd : fd to configure.
 * @return 0 in case of success, negative errno value in case of error.
 */
static inline int fd_set_close_on_exec(int fd)
{
	int old = 0, res = 0;

	old = fcntl(fd, F_GETFD, 0);
	if (old < 0) {
		res = -errno;
		POMP_LOG_FD_ERRNO("fcntl.GETFD", fd);
		goto out;
	}

	res = fcntl(fd, F_SETFD, FD_CLOEXEC | old);
	if (res < 0) {
		res = -errno;
		POMP_LOG_FD_ERRNO("fcntl.SETFD", fd);
		goto out;
	}

out:
	return res;
}

/**
 * Add flags to fd.
 * @param fd : fd to configure.
 * @param flags : flags to add.
 * @return 0 in case of success, negative errno value in case of error.
 */
static inline int fd_add_flags(int fd, int flags)
{
	int old = 0, res = 0;

	old = fcntl(fd, F_GETFL, 0);
	if (old < 0) {
		res = -errno;
		POMP_LOG_FD_ERRNO("fcntl.GETFL", fd);
		goto out;
	}

	res = fcntl(fd, F_SETFL, old | flags);
	if (res < 0) {
		res = -errno;
		POMP_LOG_FD_ERRNO("fcntl.SETFL", fd);
		goto out;
	}

out:
	return res;
}

/**
 * Setup FD_CLOEXEC and O_NONBLOCK on fd.
 * @param fd : fd to configure.
 * @return 0 in case of success, negative errno value in case of error.
 */
static inline int fd_setup_flags(int fd)
{
	int res = 0;
	res = fd_set_close_on_exec(fd);
	if (res < 0)
		goto out;
	res = fd_add_flags(fd, O_NONBLOCK);
	if (res < 0)
		goto out;
out:
	return res;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_POMP_PRIV_H_ */
