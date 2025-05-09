/**
 * @file pomp_ctx.c
 *
 * @brief Handle client/server context, socket listen/connect and events.
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

/** Maximum number of active connections for a server */
#define POMP_SERVER_MAX_CONN_COUNT	32

/** Next bind attempt delay for server (in ms) */
#define POMP_SERVER_RECONNECT_DELAY	2000

/** Reconnection delay for client (in ms) */
#define POMP_CLIENT_RECONNECT_DELAY	2000

/** Next bind attempt for dgram (in ms) */
#define POMP_DGRAM_RECONNECT_DELAY	2000

/** Determine if a socket address family is TCP/IP */
#define POMP_IS_INET(_family) \
	((_family) == AF_INET || (_family) == AF_INET6)

/** Get path of unix socket address */
#define POMP_GET_UNIX_PATH(_addr) \
	(((const struct sockaddr_un *)(_addr))->sun_path)

/** Check if a connection error should be logged, ignore common errors */
#define POMP_SHOULD_LOG_CONNECT_ERROR(_err) \
	( \
		(_err) != ECONNREFUSED && \
		(_err) != EHOSTUNREACH && \
		(_err) != EHOSTDOWN && \
		(_err) != ENETUNREACH && \
		(_err) != ENETDOWN && \
		(_err) != ENOENT && \
		(_err) != ETIMEDOUT \
	)

/** Check if a connection error means it is in progress */
#define POMP_CONNECT_IN_PROGRESS(_err) \
	( \
		(_err) == EINPROGRESS || \
		(_err) == EWOULDBLOCK \
	)

/** Context type*/
enum pomp_ctx_type {
	POMP_CTX_TYPE_SERVER = 0,	/**< Server (inet-tcp, unix) */
	POMP_CTX_TYPE_CLIENT,		/**< Client (inet-tcp, unix) */
	POMP_CTX_TYPE_DGRAM,		/**< Connection-less (inet-udp) */
};

/** Client/Server context */
struct pomp_ctx {
	/** Type of context */
	enum pomp_ctx_type	type;

	/** Function to call for connection/disconnection and messages */
	pomp_event_cb_t		eventcb;

	/** User data for event callback */
	void			*userdata;

	/** Fd loop */
	struct pomp_loop	*loop;

	/** 0 if loop is internal, 1 if external */
	int			extloop;

	/** 0 for normal context, 1, for raw context */
	int			israw;

	/** Mode to set the unix socket
	 ** (0 to use default value based on umask) */
	uint32_t		mode;

	/** Function to call for raw data reception in raw mode */
	pomp_ctx_raw_cb_t	rawcb;

	/** Function to call when sockets are created */
	pomp_socket_cb_t	sockcb;

	/** Function to call when send operation are completed */
	pomp_send_cb_t		sendcb;

	/** Timer for connection retries */
	struct pomp_timer	*timer;

	/** Socket address */
	struct sockaddr		*addr;

	/** Size of socket address */
	uint32_t		addrlen;

	/** Prevent event processing during stop and destroy */
	int			stopping;

	/** Prevent synchronous stop/destroy during notifications */
	int			notifying;

	/** Default read buffer len */
	size_t readbuf_len;

	/** Pre-allocated message for sending operation */
	struct pomp_msg		*sendmsg;

	/** Keepalive settings */
	struct {
		int		enable;
		int		idle;
		int		interval;
		int		count;
	} keepalive;

	/** maximum number of active connections for a server */
	size_t max_conn_count;

	/** Client/Server specific parameters */
	union {
		/** Server specific parameters */
		struct {
			/** Socket fd */
			int			fd;
			/** List of current connections with clients */
			struct pomp_conn	*conns;
			/** Number of connections */
			uint32_t		conncount;

			/** Bound local address */
			struct sockaddr_storage	local_addr;

			/** Bound local address size */
			socklen_t		local_addrlen;
		} server;

		/** Client specific parameters */
		struct {
			/** Socket fd */
			int			fd;
			/** Current connection with server */
			struct pomp_conn	*conn;
		} client;

		/** Dgram specific parameters */
		struct {
			/** Socket fd */
			int			fd;
			/** Fake connection object that will handle I/O */
			struct pomp_conn	*conn;

			/** Bound local address */
			struct sockaddr_storage	local_addr;

			/** Bound local address size */
			socklen_t		local_addrlen;
		} dgram;
	} u;
};

/**
 * Setup keep alive for inet socket fd.
 * @param ctx : context.
 * @param fd : fd to configure.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int fd_socket_setup_keepalive(struct pomp_ctx *ctx, int fd)
{
	int res = 0;

	/* Activate keepalive ? */
	int keepalive = ctx->keepalive.enable;
	if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive,
			sizeof(keepalive)) < 0) {
		res = -errno;
		POMP_LOG_FD_ERRNO("setsockopt.SO_KEEPALIVE", fd);
		goto out;
	}

#if defined(_WIN32) && defined(SIO_KEEPALIVE_VALS)
	/* keepalivetime : timeout (in milliseconds) with no activity until the
	 * first keep-alive packet is sent.
	 * keepaliveinterval : interval (in milliseconds) between when
	 * successive keep-alive packets are sent if no acknowledgment is
	 * received.
	 * On Windows Vista and later, the number of keep-alive probes
	 * (data retransmissions) is set to 10 and cannot be changed.
	 * On Windows Server 2003, Windows XP, and Windows 2000, the default
	 * setting for number of keep-alive probes is 5.
	 */
	{
		DWORD dwres = 0;
		struct tcp_keepalive ka;
		memset(&ka, 0, sizeof(ka));
		ka.onoff = keepalive;
		ka.keepalivetime = ctx->keepalive.idle * 1000;
		ka.keepaliveinterval = ctx->keepalive.interval * 1000;
		if (WSAIoctl(fd, SIO_KEEPALIVE_VALS, &ka, sizeof(ka),
				NULL, 0, &dwres, NULL, NULL) < 0) {
			res = -errno;
			POMP_LOG_FD_ERRNO("WSAIoctl.SIO_KEEPALIVE_VALS", fd);
			goto out;
		}
	}
#elif defined(TCP_KEEPIDLE) && defined(TCP_KEEPINTVL) && defined(TCP_KEEPCNT)
	/*
	 * TCP_KEEPIDLE  : Start keepalives after this period (in seconds)
	 * TCP_KEEPINTVL : Interval between keepalives (in seconds)
	 * TCP_KEEPCNT   : Number of keepalives before death
	 */
	if (keepalive) {
		int keepidle = ctx->keepalive.idle;
		int keepinterval = ctx->keepalive.interval;
		int keepcount = ctx->keepalive.count;

		if (setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &keepidle,
				sizeof(keepidle)) < 0) {
			res = -errno;
			POMP_LOG_FD_ERRNO("setsockopt.TCP_KEEPIDLE", fd);
			goto out;
		}

		if (setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &keepinterval,
				sizeof(keepinterval)) < 0) {
			res = -errno;
			POMP_LOG_FD_ERRNO("setsockopt.TCP_KEEPINTVL", fd);
			goto out;
		}

		if (setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &keepcount,
				sizeof(keepcount)) < 0) {
			res = -errno;
			POMP_LOG_FD_ERRNO("setsockopt.TCP_KEEPCNT", fd);
			goto out;
		}
	}
#endif

out:
	return res;
}

/**
 * Accept a new connection in a server context.
 * The user will be notified and the connection fd will be monitored for io.
 * @param server_fd : file descriptor accepting connection
 * @param ctx : context.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int server_accept_conn(int server_fd, struct pomp_ctx *ctx)
{
	int res = 0;
	int fd = -1;
	struct pomp_conn *conn = NULL;

	/* Accept connection */
	fd = accept(server_fd, NULL, NULL);
	if (fd < 0) {
		res = -errno;
		POMP_LOG_FD_ERRNO("accept", server_fd);
		goto error;
	}

	if (ctx->type != POMP_CTX_TYPE_SERVER) {
		res = 0;
		POMP_LOGE("Invalid server context");
		goto error;
	}

	/* Discard connection on closed ctx */
	if (ctx->u.server.fd == -1) {
		res = 0;
		POMP_LOGI("Server context closed");
		goto error;
	}

	if (ctx->u.server.fd != server_fd) {
		res = 0;
		POMP_LOGE("Mismatch server context fd (%d != %d)",
			  ctx->u.server.fd, server_fd);
		goto error;
	}

	/* If maximum number of connection is reached, close fd immediately */
	if (ctx->u.server.conncount >= ctx->max_conn_count) {
		POMP_LOGI("Maximum number of connections reached");
		close(fd);
		return 0;
	}

	/* Notify application */
	if (ctx->sockcb != NULL)
		(*ctx->sockcb)(ctx, fd, POMP_SOCKET_KIND_PEER, ctx->userdata);

	/* Setup socket flags */
	res = fd_setup_flags(fd);
	if (res < 0)
		goto error;

	/* Enable keep alive for TCP/IP sockets */
	if (POMP_IS_INET(ctx->addr->sa_family))
		fd_socket_setup_keepalive(ctx, fd);

	/* Allocate connection structure, transfer ownership of fd */
	conn = pomp_conn_new(ctx, ctx->loop, fd, 0, ctx->israw,
		ctx->readbuf_len);
	if (conn == NULL) {
		res = -ENOMEM;
		goto error;
	}
	fd = -1;

	/* Add in list */
	pomp_conn_set_next(conn, ctx->u.server.conns);
	ctx->u.server.conns = conn;
	ctx->u.server.conncount++;

	/* Notify user */
	pomp_ctx_notify_event(ctx, POMP_EVENT_CONNECTED, conn);
	return 0;

	/* Cleanup in case of error */
error:
	if (fd >= 0)
		close(fd);
	return res;
}

/**
 * Function called when the server socket fd is ready for events.
 * @param fd : triggered fd.
 * @param revents : event that occurred.
 * @param userdata : context object.
 */
static void server_cb(int fd, uint32_t revents, void *userdata)
{
	struct pomp_ctx *ctx = userdata;
	int res;
	int error = 0;
	socklen_t errorlen = sizeof(error);

	if (!revents)
		POMP_LOGE("unexpected non event(fd=%d)", fd);

	if (revents & POMP_FD_EVENT_HUP)
		POMP_LOGE("unexpected hup event(fd=%d)", fd);

	if (revents & POMP_FD_EVENT_ERR) {
		res = getsockopt(fd,
				 SOL_SOCKET, SO_ERROR,
				 &error, &errorlen);
		if (res < 0) {
			POMP_LOG_FD_ERRNO("getsockopt", fd);
		} else if (errorlen != (socklen_t)sizeof(error)) {
			POMP_LOGE("error event(fd=%d) err=?(?)",
				  fd);
		} else {
#ifdef _WIN32
			error = pomp_win32_error_to_errno(error);
#endif /* _WIN32 */
			POMP_LOGE("error event(fd=%d) err=%d(%s)",
				  fd,
				  error, strerror(error));
		}
	}

	if (revents & POMP_FD_EVENT_OUT)
		POMP_LOGE("unexpected write event(fd=%d)", fd);

	/* Handle incoming connection */
	if (revents & POMP_FD_EVENT_IN)
		server_accept_conn(fd, ctx);
}

/**
 * Start a server context.
 * It will start listening for incoming connection.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int server_start(struct pomp_ctx *ctx)
{
	int res = 0;
	int sockopt = 0;

	/* Create server socket */
	ctx->u.server.fd = socket(ctx->addr->sa_family, SOCK_STREAM, 0);
	if (ctx->u.server.fd < 0) {
		res = -errno;
		POMP_LOG_ERRNO("socket");
		goto error;
	}

	/* Notify application */
	if (ctx->sockcb != NULL) {
		(*ctx->sockcb)(ctx, ctx->u.server.fd,
				POMP_SOCKET_KIND_SERVER, ctx->userdata);
	}

	/* Setup socket flags */
	res = fd_setup_flags(ctx->u.server.fd);
	if (res < 0)
		goto error;

	/* Allow reuse of address */
	sockopt = 1;
	if (setsockopt(ctx->u.server.fd, SOL_SOCKET, SO_REUSEADDR,
			&sockopt, sizeof(sockopt)) < 0) {
		res = -errno;
		POMP_LOG_FD_ERRNO("setsockopt.SO_REUSEADDR", ctx->u.server.fd);
		goto error;
	}

	/* For non abstract unix socket, unlink file before bind */
	if (ctx->addr->sa_family == AF_UNIX
			&& POMP_GET_UNIX_PATH(ctx->addr)[0] != '\0') {
		unlink(POMP_GET_UNIX_PATH(ctx->addr));
	}

	/* Bind to address  */
	if (bind(ctx->u.server.fd, ctx->addr, ctx->addrlen) < 0) {
		/* Handle case where address do not match an existent
		 * interface to try again later */
		if (errno != EADDRNOTAVAIL) {
			res = -errno;
			POMP_LOG_FD_ERRNO("bind", ctx->u.server.fd);
			goto error;
		}
		/* Free resources, try again later */
		close(ctx->u.server.fd);
		ctx->u.server.fd = -1;
		res = pomp_timer_set(ctx->timer, POMP_SERVER_RECONNECT_DELAY);
		if (res < 0)
			goto error;
		return 0;
	}

	/* Change access mode for non abstract unix socket */
#ifndef _WIN32
	if (ctx->addr->sa_family == AF_UNIX
			&& POMP_GET_UNIX_PATH(ctx->addr)[0] != '\0'
			&& ctx->mode != 0) {
		if (chmod(POMP_GET_UNIX_PATH(ctx->addr), ctx->mode) < 0) {
			res = -errno;
			POMP_LOG_ERRNO("chmod");
			goto error;
		}
	}
#endif /* !_WIN32 */

	/* Get local address information */
	ctx->u.server.local_addrlen = sizeof(ctx->u.server.local_addr);
	if (getsockname(ctx->u.server.fd,
			(struct sockaddr *)&ctx->u.server.local_addr,
			&ctx->u.server.local_addrlen) < 0) {
		POMP_LOG_FD_ERRNO("getsockname", ctx->u.server.fd);
		ctx->u.server.local_addrlen = 0;
	}

	/* Start listening */
	if (listen(ctx->u.server.fd, SOMAXCONN) < 0) {
		res = -errno;
		POMP_LOG_FD_ERRNO("listen", ctx->u.server.fd);
		goto error;
	}

	/* Add to loop */
	res = pomp_loop_add(ctx->loop, ctx->u.server.fd, POMP_FD_EVENT_IN,
			&server_cb, ctx);
	if (res < 0)
		goto error;

	return 0;

	/* Cleanup in case of error */
error:
	if (ctx->u.server.fd >= 0) {
		if (pomp_loop_has_fd(ctx->loop, ctx->u.server.fd))
			pomp_loop_remove(ctx->loop, ctx->u.server.fd);
		close(ctx->u.server.fd);
		ctx->u.server.fd = -1;
		memset(&ctx->u.server.local_addr, 0,
				sizeof(ctx->u.server.local_addr));
		ctx->u.server.local_addrlen = 0;
	}
	return res;
}

/**
 * Stop server context.
 * It will disconnect (with notification) all connections and free all internal
 * resources.
 * @param ctx : context.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int server_stop(struct pomp_ctx *ctx)
{
	/* Remove all connections */
	while (ctx->u.server.conns != NULL)
		pomp_ctx_remove_conn(ctx, ctx->u.server.conns);

	/* Stop server socket */
	if (ctx->u.server.fd >= 0) {
		pomp_loop_remove(ctx->loop, ctx->u.server.fd);
		close(ctx->u.server.fd);
		ctx->u.server.fd = -1;
	}

	/* For non abstract unix socket, unlink file also */
	if (ctx->addr->sa_family == AF_UNIX
			&& POMP_GET_UNIX_PATH(ctx->addr)[0] != '\0') {
		unlink(POMP_GET_UNIX_PATH(ctx->addr));
	}

	/* Clear bound local address */
	memset(&ctx->u.server.local_addr, 0, sizeof(ctx->u.server.local_addr));
	ctx->u.server.local_addrlen = 0;

	return 0;
}

/**
 * Complete the client connection with the server.
 * If connection is successful, user will be notified and the connection fd will
 * be monitored for io. Otherwise reconnection will be scheduled.
 * @param client_fd : file descriptor requesting connection
 * @param ctx : context.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int client_complete_conn(int client_fd, struct pomp_ctx *ctx)
{
	int sockerr = 0;
	socklen_t sockerrlen = 0;
	struct pomp_conn *conn = NULL;

	/* Remove fd from loop now so it can be added by connection object */
	pomp_loop_remove(ctx->loop, client_fd);

	if (ctx->type != POMP_CTX_TYPE_CLIENT) {
		POMP_LOGE("Invalid client context");
		goto error;
	}

	if (ctx->u.client.fd == -1) {
		POMP_LOGI("Client context closed");
		goto error;
	}

	if (ctx->u.client.fd != client_fd) {
		POMP_LOGE("Mismatch client context fd (%d != %d)",
			  ctx->u.client.fd, client_fd);
		goto error;
	}

	/* Get connection result */
	sockerrlen = sizeof(sockerr);
	if (getsockopt(client_fd, SOL_SOCKET, SO_ERROR,
			(char *)&sockerr, &sockerrlen) < 0) {
		POMP_LOG_FD_ERRNO("getsockopt.SO_ERROR", client_fd);
		goto reconnect;
	}
#ifdef _WIN32
	sockerr = pomp_win32_error_to_errno(sockerr);
#endif /* _WIN32 */

	/* Reconnect in case of error */
	if (sockerr != 0) {
		if (POMP_SHOULD_LOG_CONNECT_ERROR(sockerr)) {
			char buf[128] = "";
			pomp_addr_format(buf, sizeof(buf),
					ctx->addr, ctx->addrlen);
			POMP_LOGE("connect(async)(fd=%d)(addr=%s) err=%d(%s)",
					client_fd, buf,
					sockerr, strerror(sockerr));
		}
		goto reconnect;
	}

	/* Enable keep alive for TCP/IP sockets */
	if (POMP_IS_INET(ctx->addr->sa_family))
		fd_socket_setup_keepalive(ctx, client_fd);

	/* Allocate connection structure */
	conn = pomp_conn_new(ctx, ctx->loop, client_fd, 0, ctx->israw,
		ctx->readbuf_len);
	if (conn == NULL)
		goto reconnect;

	/* Save connection, transfer ownership of fd */
	ctx->u.client.conn = conn;
	ctx->u.client.fd = -1;

	/* Notify user */
	pomp_ctx_notify_event(ctx, POMP_EVENT_CONNECTED, conn);
	return 0;

	/* Cleanup and reconnect in case of error */
reconnect:
	/* fd already removed from loop */
	close(client_fd);
	ctx->u.client.fd = -1;

	/* Try a reconnection */
	return pomp_timer_set(ctx->timer, POMP_CLIENT_RECONNECT_DELAY);

error:
	/* fd already removed from loop */
	close(client_fd);
	return 0;
}

/**
 * Function called when the client socket fd is ready for events.
 * @param fd : triggered fd.
 * @param revents : event that occurred.
 * @param userdata : context object.
 */
static void client_cb(int fd, uint32_t revents, void *userdata)
{
	struct pomp_ctx *ctx = userdata;

	/* Handle connection completion */
	client_complete_conn(fd, ctx);
}

/**
 * Start a client context.
 * It will start connecting to server and automatically reconnect in case of
 * timeout or disconnection.
 * @param ctx : context.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int client_start(struct pomp_ctx *ctx)
{
	int res = 0;

	/* Create client socket */
	ctx->u.client.fd = socket(
		ctx->addr->sa_family,
#ifdef AF_QIPCRTR
		ctx->addr->sa_family == AF_QIPCRTR ? SOCK_DGRAM : SOCK_STREAM,
#else
		SOCK_STREAM,
#endif
		0);
	if (ctx->u.client.fd < 0) {
		res = -errno;
		POMP_LOG_ERRNO("socket");
		goto error;
	}

	/* Notify application */
	if (ctx->sockcb != NULL) {
		(*ctx->sockcb)(ctx, ctx->u.client.fd,
				POMP_SOCKET_KIND_CLIENT, ctx->userdata);
	}

	/* Setup socket flags */
	res = fd_setup_flags(ctx->u.client.fd);
	if (res < 0)
		goto error;

	/* Add to loop to monitor connection completion */
	res = pomp_loop_add(ctx->loop, ctx->u.client.fd, POMP_FD_EVENT_OUT,
			&client_cb, ctx);
	if (res < 0)
		goto error;

	/* Connect to address */
	res = connect(ctx->u.client.fd, ctx->addr, ctx->addrlen);

	/* If error and not pending, try again later */
	if (res != 0 && !POMP_CONNECT_IN_PROGRESS(errno)) {
		if (POMP_SHOULD_LOG_CONNECT_ERROR(errno)) {
			char buf[128] = "";
			res = -errno;
			pomp_addr_format(buf, sizeof(buf),
					ctx->addr, ctx->addrlen);
			POMP_LOGE("connect(fd=%d)(addr=%s) err=%d(%s)",
					ctx->u.client.fd, buf,
					-res, strerror(-res));
		}
		/* Close socket and try again later */
		pomp_loop_remove(ctx->loop, ctx->u.client.fd);
		close(ctx->u.client.fd);
		ctx->u.client.fd = -1;
		res = pomp_timer_set(ctx->timer, POMP_CLIENT_RECONNECT_DELAY);
		if (res < 0)
			goto error;
		return 0;
	}

	/* Notification will be done later when the socket is writable
	 * (even if it succeeded now, notify asynchronously) */
	return 0;

	/* Cleanup in case of error */
error:
	if (ctx->u.client.fd >= 0) {
		pomp_loop_remove(ctx->loop, ctx->u.client.fd);
		close(ctx->u.client.fd);
		ctx->u.client.fd = -1;
	}
	return res;
}

/**
 * Stop client context.
 * It will disconnect (with notification) current connection if any and free
 * all internal resources.
 * @param ctx : context.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int client_stop(struct pomp_ctx *ctx)
{
	/* Remove current connection */
	if (ctx->u.client.conn != NULL)
		pomp_ctx_remove_conn(ctx, ctx->u.client.conn);

	/* Abort current connection in any */
	if (ctx->u.client.fd >= 0) {
		pomp_loop_remove(ctx->loop, ctx->u.client.fd);
		close(ctx->u.client.fd);
		ctx->u.client.fd = -1;
	}

	return 0;
}

/**
 * Start a dgram context.
 * It will bind to the context address.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int dgram_start(struct pomp_ctx *ctx)
{
	int res = 0;
	int sockopt = 0;
	struct pomp_conn *conn = NULL;

	/* Create dgram socket */
	ctx->u.dgram.fd = socket(ctx->addr->sa_family, SOCK_DGRAM, 0);
	if (ctx->u.dgram.fd < 0) {
		res = -errno;
		POMP_LOG_ERRNO("socket");
		goto error;
	}

	/* Notify application */
	if (ctx->sockcb != NULL) {
		(*ctx->sockcb)(ctx, ctx->u.dgram.fd,
				POMP_SOCKET_KIND_DGRAM, ctx->userdata);
	}

	/* Setup socket flags */
	res = fd_setup_flags(ctx->u.dgram.fd);
	if (res < 0)
		goto error;

	/* Allow reuse of address */
	sockopt = 1;
	if (setsockopt(ctx->u.dgram.fd, SOL_SOCKET, SO_REUSEADDR,
			&sockopt, sizeof(sockopt)) < 0) {
		res = -errno;
		POMP_LOG_FD_ERRNO("setsockopt.SO_REUSEADDR", ctx->u.dgram.fd);
		goto error;
	}

	/* Bind to address  */
	if (bind(ctx->u.dgram.fd, ctx->addr, ctx->addrlen) < 0) {
		/* Handle case where address do not match an existent
		 * interface to try again later */
		if (errno != EADDRNOTAVAIL) {
			res = -errno;
			POMP_LOG_FD_ERRNO("bind", ctx->u.dgram.fd);
			goto error;
		}
		goto reconnect;
	}

	/* Get local address information */
	ctx->u.dgram.local_addrlen = sizeof(ctx->u.dgram.local_addr);
	if (getsockname(ctx->u.dgram.fd,
			(struct sockaddr *)&ctx->u.dgram.local_addr,
			&ctx->u.dgram.local_addrlen) < 0) {
		POMP_LOG_FD_ERRNO("getsockname", ctx->u.dgram.fd);
		ctx->u.dgram.local_addrlen = 0;
	}

	/* Allocate connection structure */
	conn = pomp_conn_new(ctx, ctx->loop, ctx->u.dgram.fd, 1, ctx->israw,
		ctx->readbuf_len);
	if (conn == NULL)
		goto reconnect;

	/* Save connection, transfer ownership of fd */
	ctx->u.dgram.conn = conn;
	ctx->u.dgram.fd = -1;

	return 0;

	/* Cleanup and reconnect  */
reconnect:
	if (ctx->u.dgram.fd >= 0) {
		close(ctx->u.dgram.fd);
		ctx->u.dgram.fd = -1;
		memset(&ctx->u.dgram.local_addr, 0,
				sizeof(ctx->u.dgram.local_addr));
		ctx->u.dgram.local_addrlen = 0;
	}

	/* Try a reconnection */
	return pomp_timer_set(ctx->timer, POMP_DGRAM_RECONNECT_DELAY);

	/* Cleanup in case of error */
error:
	if (ctx->u.dgram.fd >= 0) {
		close(ctx->u.dgram.fd);
		ctx->u.dgram.fd = -1;
		memset(&ctx->u.dgram.local_addr, 0,
				sizeof(ctx->u.dgram.local_addr));
		ctx->u.dgram.local_addrlen = 0;
	}
	return res;
}

/**
 * Stop a dgram context.
 * @param ctx : context.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int dgram_stop(struct pomp_ctx *ctx)
{
	/* Remove fake connection */
	if (ctx->u.dgram.conn != NULL)
		pomp_ctx_remove_conn(ctx, ctx->u.dgram.conn);

	/* Unbind socket (normally the fd was owned by fake connection) */
	if (ctx->u.dgram.fd >= 0) {
		close(ctx->u.dgram.fd);
		ctx->u.dgram.fd = -1;
	}

	/* Clear bound local address */
	memset(&ctx->u.dgram.local_addr, 0, sizeof(ctx->u.dgram.local_addr));
	ctx->u.dgram.local_addrlen = 0;

	return 0;
}

/**
 * Function called when the timer is triggered.
 * @param timer : timer
 * @param userdata : context object.
 */
static void timer_cb(struct pomp_timer *timer, void *userdata)
{
	struct pomp_ctx *ctx = userdata;

	/* Clear timer */
	pomp_timer_clear(ctx->timer);

	/* Try reconnecting the client/server */
	switch (ctx->type) {
	case POMP_CTX_TYPE_SERVER:
		server_start(ctx);
		break;

	case POMP_CTX_TYPE_CLIENT:
		client_start(ctx);
		break;

	case POMP_CTX_TYPE_DGRAM:
		dgram_start(ctx);
		break;
	}
}

/*
 * See documentation in public header.
 */
const char *pomp_event_str(enum pomp_event event)
{
	switch (event) {
	case POMP_EVENT_CONNECTED: return "CONNECTED";
	case POMP_EVENT_DISCONNECTED: return "DISCONNECTED";
	case POMP_EVENT_MSG: return "MSG";
	default: return "UNKNOWN";
	}
}

/*
 * See documentation in public header.
 */
const char *pomp_socket_kind_str(enum pomp_socket_kind kind)
{
	switch (kind) {
	case POMP_SOCKET_KIND_SERVER: return "SERVER";
	case POMP_SOCKET_KIND_PEER: return "PEER";
	case POMP_SOCKET_KIND_CLIENT: return "CLIENT";
	case POMP_SOCKET_KIND_DGRAM: return "DGRAM";
	default: return "UNKNOWN";
	}
}

/*
 * See documentation in public header.
 */
struct pomp_ctx *pomp_ctx_new(pomp_event_cb_t cb, void *userdata)
{
	struct pomp_ctx *ctx = NULL;
	struct pomp_loop *loop = NULL;

	/* Create a loop */
	loop = pomp_loop_new();
	if (loop == NULL)
		goto error;

	/* Create context with the loop */
	ctx = pomp_ctx_new_with_loop(cb, userdata, loop);
	if (ctx == NULL)
		goto error;

	/* The loop is actually internal, not external now */
	ctx->extloop = 0;
	return ctx;

	/* Cleanup in case of error */
error:
	if (loop != NULL)
		pomp_loop_destroy(loop);
	return NULL;
}

/*
 * See documentation in public header.
 */
struct pomp_ctx *pomp_ctx_new_with_loop(pomp_event_cb_t cb,
		void *userdata, struct pomp_loop *loop)
{
	struct pomp_ctx *ctx = NULL;

	POMP_RETURN_VAL_IF_FAILED(loop != NULL, -EINVAL, NULL);
	POMP_LOOP_CHECK_OWNER(loop);

	/* Allocate context structure */
	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL)
		goto error;

	/* Save parameters */
	ctx->eventcb = cb;
	ctx->userdata = userdata;
	ctx->loop = loop;
	ctx->extloop = 1;
	ctx->israw = 0;

	/* Default keepalive settings */
	ctx->keepalive.enable = 1;
	ctx->keepalive.idle = 5;
	ctx->keepalive.interval = 1;
	ctx->keepalive.count = 2;
	ctx->readbuf_len = POMP_CONN_READ_SIZE;

	ctx->max_conn_count = POMP_SERVER_MAX_CONN_COUNT;

	/* Pre-allocate a message for sending operation */
	ctx->sendmsg = pomp_msg_new();
	if (ctx->sendmsg == NULL)
		goto error;

	/* Allocate timer */
	ctx->timer = pomp_timer_new(ctx->loop, &timer_cb, ctx);
	if (ctx->timer == NULL)
		goto error;

	return ctx;

	/* Cleanup in case of error */
error:
	if (ctx != NULL)
		pomp_ctx_destroy(ctx);
	return NULL;
}

/*
 * See documentation in public header.
 */
int pomp_ctx_set_raw(struct pomp_ctx *ctx, pomp_ctx_raw_cb_t cb)
{
	POMP_RETURN_ERR_IF_FAILED(ctx != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(cb != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(ctx->addr == NULL, -EBUSY);
	POMP_LOOP_CHECK_OWNER(ctx->loop);
	ctx->israw = 1;
	ctx->rawcb = cb;
	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_ctx_set_socket_cb(struct pomp_ctx *ctx, pomp_socket_cb_t cb)
{
	POMP_RETURN_ERR_IF_FAILED(ctx != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(cb != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(ctx->addr == NULL, -EBUSY);
	POMP_LOOP_CHECK_OWNER(ctx->loop);
	ctx->sockcb = cb;
	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_ctx_set_send_cb(struct pomp_ctx *ctx, pomp_send_cb_t cb)
{
	POMP_RETURN_ERR_IF_FAILED(ctx != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(cb != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(ctx->addr == NULL, -EBUSY);
	POMP_LOOP_CHECK_OWNER(ctx->loop);
	ctx->sendcb = cb;
	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_ctx_setup_keepalive(struct pomp_ctx *ctx, int enable,
		int idle, int interval, int count)
{
	POMP_RETURN_ERR_IF_FAILED(ctx != NULL, -EINVAL);
	POMP_LOOP_CHECK_OWNER(ctx->loop);
	ctx->keepalive.enable = enable;
	ctx->keepalive.idle = idle;
	ctx->keepalive.interval = interval;
	ctx->keepalive.count = count;
	return 0;
}

/*
 * See documentation in public header.
 */
POMP_API int pomp_ctx_set_max_conn(struct pomp_ctx *ctx, size_t count)
{
	POMP_RETURN_ERR_IF_FAILED(ctx != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(count > 0, -EINVAL);
	POMP_LOOP_CHECK_OWNER(ctx->loop);

	ctx->max_conn_count = count;

	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_ctx_destroy(struct pomp_ctx *ctx)
{
	POMP_RETURN_ERR_IF_FAILED(ctx != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(ctx->addr == NULL, -EBUSY);
	POMP_LOOP_CHECK_OWNER(ctx->loop);
	if (ctx->sendmsg != NULL)
		pomp_msg_destroy(ctx->sendmsg);
	if (ctx->timer != NULL)
		pomp_timer_destroy(ctx->timer);
	if (ctx->loop != NULL && !ctx->extloop)
		pomp_loop_destroy(ctx->loop);
	free(ctx);
	return 0;
}

/**
 * Internal start function.
 * @param ctx : context.
 * @param type : context type.
 * @param addr : local address to listen on.
 * @param addrlen : local address size.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int pomp_ctx_start(struct pomp_ctx *ctx, enum pomp_ctx_type type,
		const struct sockaddr *addr, uint32_t addrlen)
{
	int res = 0;
	POMP_RETURN_ERR_IF_FAILED(ctx != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(addr != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(ctx->addr == NULL, -EBUSY);
	POMP_LOOP_CHECK_OWNER(ctx->loop);

	/* Copy address */
	ctx->addr = malloc(addrlen);
	if (ctx->addr == NULL)
		return -ENOMEM;
	ctx->addrlen = addrlen;
	memcpy(ctx->addr, addr, addrlen);

	/* Save type */
	ctx->type = type;

	/* Setup server/client/dgram specific stuff */
	switch (ctx->type) {
	case POMP_CTX_TYPE_SERVER:
		ctx->u.server.fd = -1;
		ctx->u.server.conns = NULL;
		ctx->u.server.conncount = 0;
		memset(&ctx->u.server.local_addr, 0,
				sizeof(ctx->u.server.local_addr));
		ctx->u.server.local_addrlen = 0;
		res = server_start(ctx);
		break;

	case POMP_CTX_TYPE_CLIENT:
		ctx->u.client.fd = -1;
		ctx->u.client.conn = NULL;
		res = client_start(ctx);
		break;

	case POMP_CTX_TYPE_DGRAM:
		ctx->u.dgram.fd = -1;
		memset(&ctx->u.dgram.local_addr, 0,
				sizeof(ctx->u.dgram.local_addr));
		ctx->u.dgram.local_addrlen = 0;
		res = dgram_start(ctx);
		break;

	default:
		res = -EINVAL;
		break;
	}

	/* Cleanup address in case of error */
	if (res < 0) {
		free(ctx->addr);
		ctx->addr = NULL;
		ctx->addrlen = 0;
	}
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_ctx_listen(struct pomp_ctx *ctx,
		const struct sockaddr *addr, uint32_t addrlen)
{
	POMP_RETURN_ERR_IF_FAILED(ctx != NULL, -EINVAL);
	POMP_LOOP_CHECK_OWNER(ctx->loop);
	ctx->mode = 0;
	return pomp_ctx_start(ctx, POMP_CTX_TYPE_SERVER, addr, addrlen);
}

/*
 * See documentation in public header.
 */
int pomp_ctx_listen_with_access_mode(struct pomp_ctx *ctx,
		const struct sockaddr *addr, uint32_t addrlen, uint32_t mode)
{
	POMP_RETURN_ERR_IF_FAILED(ctx != NULL, -EINVAL);
	POMP_LOOP_CHECK_OWNER(ctx->loop);
	ctx->mode = mode;
	return pomp_ctx_start(ctx, POMP_CTX_TYPE_SERVER, addr, addrlen);
}

/*
 * See documentation in public header.
 */
int pomp_ctx_connect(struct pomp_ctx *ctx,
		const struct sockaddr *addr, uint32_t addrlen)
{
	POMP_RETURN_ERR_IF_FAILED(ctx != NULL, -EINVAL);
	POMP_LOOP_CHECK_OWNER(ctx->loop);
	return pomp_ctx_start(ctx, POMP_CTX_TYPE_CLIENT, addr, addrlen);
}

/*
 * See documentation in public header.
 */
int pomp_ctx_bind(struct pomp_ctx *ctx,
		const struct sockaddr *addr, uint32_t addrlen)
{
	POMP_RETURN_ERR_IF_FAILED(ctx != NULL, -EINVAL);
	POMP_LOOP_CHECK_OWNER(ctx->loop);
	return pomp_ctx_start(ctx, POMP_CTX_TYPE_DGRAM, addr, addrlen);
}

/**
 * Asynchronous version of pomp_ctx_stop called when loop is idle to break
 * recursion of stop.
 * @param userdata : pomp context.
 */
static void pomp_ctx_stop_idle(void *userdata)
{
	struct pomp_ctx *ctx = userdata;

	/* Stop server/client/dgram */
	switch (ctx->type) {
	case POMP_CTX_TYPE_SERVER:
		server_stop(ctx);
		break;

	case POMP_CTX_TYPE_CLIENT:
		client_stop(ctx);
		break;

	case POMP_CTX_TYPE_DGRAM:
		dgram_stop(ctx);
		break;
	}

	/* Free common resources */
	pomp_timer_clear(ctx->timer);
	free(ctx->addr);
	ctx->addr = NULL;
	ctx->stopping = 0;
}

/*
 * See documentation in public header.
 */
int pomp_ctx_stop(struct pomp_ctx *ctx)
{
	POMP_RETURN_ERR_IF_FAILED(ctx != NULL, -EINVAL);
	POMP_LOOP_CHECK_OWNER(ctx->loop);
	if (ctx->addr == NULL || ctx->stopping)
		return 0;

	ctx->stopping = 1;

	/* If currently notifying events/messages, do the stop in idle,
	 * otherwise do it synchronously */
	if (ctx->notifying > 0) {
		return pomp_loop_idle_add(ctx->loop, &pomp_ctx_stop_idle, ctx);
	} else {
		pomp_ctx_stop_idle(ctx);
		return 0;
	}
}

/*
 * See documentation in public header.
 */
intptr_t pomp_ctx_get_fd(const struct pomp_ctx *ctx)
{
	POMP_RETURN_ERR_IF_FAILED(ctx != NULL, -EINVAL);
	POMP_LOOP_CHECK_OWNER(ctx->loop);
	return pomp_loop_get_fd(ctx->loop);
}

/*
 * See documentation in public header.
 */
int pomp_ctx_process_fd(struct pomp_ctx *ctx)
{
	/* No owner check */
	POMP_RETURN_ERR_IF_FAILED(ctx != NULL, -EINVAL);
	return pomp_loop_wait_and_process(ctx->loop, 0);
}

/*
 * See documentation in public header.
 */
int pomp_ctx_wait_and_process(struct pomp_ctx *ctx, int timeout)
{
	/* No owner check */
	POMP_RETURN_ERR_IF_FAILED(ctx != NULL, -EINVAL);
	return pomp_loop_wait_and_process(ctx->loop, timeout);
}

/*
 * See documentation in public header.
 * Thread safe.
 */
int pomp_ctx_wakeup(struct pomp_ctx *ctx)
{
	POMP_RETURN_ERR_IF_FAILED(ctx != NULL, -EINVAL);
	return pomp_loop_wakeup(ctx->loop);
}

/*
 * See documentation in public header.
 * Thread safe.
 */
struct pomp_loop *pomp_ctx_get_loop(struct pomp_ctx *ctx)
{
	POMP_RETURN_VAL_IF_FAILED(ctx != NULL, -EINVAL, NULL);
	return ctx->loop;
}

/*
 * See documentation in public header.
 */
struct pomp_conn *pomp_ctx_get_next_conn(const struct pomp_ctx *ctx,
		const struct pomp_conn *prev)
{
	POMP_RETURN_VAL_IF_FAILED(ctx != NULL, -EINVAL, NULL);
	POMP_RETURN_VAL_IF_FAILED(ctx->type == POMP_CTX_TYPE_SERVER,
			-EINVAL, NULL);
	POMP_LOOP_CHECK_OWNER(ctx->loop);
	return prev == NULL ? ctx->u.server.conns : pomp_conn_get_next(prev);
}

/*
 * See documentation in public header.
 */
struct pomp_conn *pomp_ctx_get_conn(const struct pomp_ctx *ctx)
{
	POMP_RETURN_VAL_IF_FAILED(ctx != NULL, -EINVAL, NULL);
	POMP_RETURN_VAL_IF_FAILED(ctx->type == POMP_CTX_TYPE_CLIENT,
			-EINVAL, NULL);
	POMP_LOOP_CHECK_OWNER(ctx->loop);
	return ctx->u.client.conn;
}

/*
 * See documentation in public header.
 */
const struct sockaddr *pomp_ctx_get_local_addr(struct pomp_ctx *ctx,
		uint32_t *addrlen)
{
	POMP_RETURN_VAL_IF_FAILED(ctx != NULL, -EINVAL, NULL);
	POMP_RETURN_VAL_IF_FAILED(addrlen != NULL, -EINVAL, NULL);
	POMP_LOOP_CHECK_OWNER(ctx->loop);

	if (ctx->type == POMP_CTX_TYPE_SERVER) {
		*addrlen = ctx->u.server.local_addrlen;
		return (const struct sockaddr *)&ctx->u.server.local_addr;
	} else if (ctx->type == POMP_CTX_TYPE_DGRAM) {
		*addrlen = ctx->u.dgram.local_addrlen;
		return (const struct sockaddr *)&ctx->u.dgram.local_addr;
	} else {
		return NULL;
	}
}

/*
 * See documentation in public header.
 */
int pomp_ctx_send_msg(struct pomp_ctx *ctx, const struct pomp_msg *msg)
{
	int res = 0;
	struct pomp_conn *conn = NULL;

	POMP_RETURN_ERR_IF_FAILED(ctx != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(msg != NULL, -EINVAL);
	POMP_LOOP_CHECK_OWNER(ctx->loop);

	switch (ctx->type) {
	case POMP_CTX_TYPE_SERVER:
		/* Broadcast to all connections, ignore errors */
		conn = ctx->u.server.conns;
		while (conn != NULL) {
			(void)pomp_conn_send_msg(conn, msg);
			conn = pomp_conn_get_next(conn);
		}
		break;

	case POMP_CTX_TYPE_CLIENT:
		/* Send if connected */
		if (ctx->u.client.conn != NULL)
			res = pomp_conn_send_msg(ctx->u.client.conn, msg);
		else
			res = -ENOTCONN;
		break;

	case POMP_CTX_TYPE_DGRAM:
		res = -ENOTCONN;
		break;
	}

	return res;
}

/*
 * See documentation in public header.
 */
int pomp_ctx_send_msg_to(struct pomp_ctx *ctx,
		const struct pomp_msg *msg,
		const struct sockaddr *addr, uint32_t addrlen)
{
	POMP_RETURN_ERR_IF_FAILED(ctx != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(addr != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(ctx->type == POMP_CTX_TYPE_DGRAM, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(ctx->u.dgram.conn != NULL, -EINVAL);
	POMP_LOOP_CHECK_OWNER(ctx->loop);

	return pomp_conn_send_msg_to(ctx->u.dgram.conn, msg, addr, addrlen);
}

/*
 * See documentation in public header.
 */
int pomp_ctx_send(struct pomp_ctx *ctx, uint32_t msgid, const char *fmt, ...)
{
	int res = 0;
	va_list args;
	va_start(args, fmt);
	res = pomp_ctx_sendv(ctx, msgid, fmt, args);
	va_end(args);
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_ctx_sendv(struct pomp_ctx *ctx, uint32_t msgid,
		const char *fmt, va_list args)
{
	int res = 0;
	POMP_RETURN_ERR_IF_FAILED(ctx != NULL, -EINVAL);
	POMP_LOOP_CHECK_OWNER(ctx->loop);

	/* Write message using pre-allocated one and send it */
	res = pomp_msg_writev(ctx->sendmsg, msgid, fmt, args);
	if (res == 0)
		res = pomp_ctx_send_msg(ctx, ctx->sendmsg);

	/* Do not clean message, to avoid re-allocation */
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_ctx_send_raw_buf(struct pomp_ctx *ctx, struct pomp_buffer *buf)
{
	int res = 0;
	struct pomp_conn *conn = NULL;

	POMP_RETURN_ERR_IF_FAILED(ctx != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(buf != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(ctx->israw, -EINVAL);
	POMP_LOOP_CHECK_OWNER(ctx->loop);

	switch (ctx->type) {
	case POMP_CTX_TYPE_SERVER:
		/* Broadcast to all connections, ignore errors */
		conn = ctx->u.server.conns;
		while (conn != NULL) {
			(void)pomp_conn_send_raw_buf(conn, buf);
			conn = pomp_conn_get_next(conn);
		}
		break;

	case POMP_CTX_TYPE_CLIENT:
		/* Send if connected */
		if (ctx->u.client.conn != NULL)
			res = pomp_conn_send_raw_buf(ctx->u.client.conn, buf);
		else
			res = -ENOTCONN;
		break;

	case POMP_CTX_TYPE_DGRAM:
		res = -ENOTCONN;
		break;
	}

	return res;
}

/*
 * See documentation in public header.
 */
int pomp_ctx_send_raw_buf_to(struct pomp_ctx *ctx, struct pomp_buffer *buf,
		const struct sockaddr *addr, uint32_t addrlen)
{
	POMP_RETURN_ERR_IF_FAILED(ctx != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(buf != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(addr != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(ctx->israw, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(ctx->type == POMP_CTX_TYPE_DGRAM, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(ctx->u.dgram.conn != NULL, -EINVAL);
	POMP_LOOP_CHECK_OWNER(ctx->loop);

	return pomp_conn_send_raw_buf_to(ctx->u.dgram.conn, buf, addr, addrlen);
}

/*
 * See documentation in public header.
 */
int pomp_ctx_set_read_buffer_len(struct pomp_ctx *ctx,
		size_t len)
{
	POMP_RETURN_ERR_IF_FAILED(ctx != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(len != 0, -EINVAL);
	POMP_LOOP_CHECK_OWNER(ctx->loop);
	ctx->readbuf_len = len;
	return 0;
}

/**
 * Remove a connection from the context.
 * @param ctx : context.
 * @param conn : connection to remove. It will be destroyed during the call so
 * don't use it after.
 * @return 0 in case of success, negative errno value in case of error.
 */
int pomp_ctx_remove_conn(struct pomp_ctx *ctx, struct pomp_conn *conn)
{
	int found = 0;
	struct pomp_conn *prev = NULL;

	POMP_RETURN_ERR_IF_FAILED(ctx != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(conn != NULL, -EINVAL);

	/* Remove from server / client */
	switch (ctx->type) {
	case POMP_CTX_TYPE_SERVER:
		if (ctx->u.server.conns == conn) {
			/* This was the first in the list */
			ctx->u.server.conns = pomp_conn_get_next(conn);
			ctx->u.server.conncount--;
			found = 1;
		} else {
			prev = ctx->u.server.conns;
			while (prev != NULL) {
				if (pomp_conn_get_next(prev) != conn) {
					prev = pomp_conn_get_next(prev);
					continue;
				}

				pomp_conn_set_next(prev,
						pomp_conn_get_next(conn));
				ctx->u.server.conncount--;
				found = 1;
				break;
			}
		}
		break;

	case POMP_CTX_TYPE_CLIENT:
		if (ctx->u.client.conn == conn) {
			ctx->u.client.conn = NULL;
			found = 1;
		}
		break;

	case POMP_CTX_TYPE_DGRAM:
		if (ctx->u.dgram.conn == conn) {
			ctx->u.dgram.conn = NULL;
			found = 1;
		}
		break;
	}

	if (!found) {
		POMP_LOGE("conn %p not found in ctx %p", conn, ctx);
	} else {
		/* Notify user */
		if (ctx->type != POMP_CTX_TYPE_DGRAM)
			pomp_ctx_notify_event(ctx,
					POMP_EVENT_DISCONNECTED, conn);

		/* Free connection itself */
		pomp_conn_close(conn);
		pomp_conn_destroy(conn);
	}

	/* Reconnect client if needed */
	if (ctx->type == POMP_CTX_TYPE_CLIENT
			&& !ctx->stopping && ctx->addr != NULL) {
		pomp_timer_set(ctx->timer, POMP_CLIENT_RECONNECT_DELAY);
	}

	return 0;
}

/**
 * Notify a connection event.
 * @param ctx : context.
 * @param event : event to notify.
 * @param conn : connection on which the event occurred.
 * @return 0 in case of success, negative errno value in case of error.
 */
int pomp_ctx_notify_event(struct pomp_ctx *ctx, enum pomp_event event,
		struct pomp_conn *conn)
{
	POMP_RETURN_ERR_IF_FAILED(ctx != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(event != POMP_EVENT_MSG, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(conn != NULL, -EINVAL);

	if (ctx->eventcb != NULL) {
		ctx->notifying++;
		(*ctx->eventcb)(ctx, event, conn, NULL, ctx->userdata);
		ctx->notifying--;
	}

	return 0;
}

/**
 * Notify a message event.
 * @param ctx : context.
 * @param conn : connection on which the message has been received.
 * @param msg : message to notify.
 * @return 0 in case of success, negative errno value in case of error.
 */
int pomp_ctx_notify_msg(struct pomp_ctx *ctx, struct pomp_conn *conn,
		const struct pomp_msg *msg)
{
	POMP_RETURN_ERR_IF_FAILED(ctx != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(conn != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(msg != NULL, -EINVAL);
	POMP_LOOP_CHECK_OWNER(ctx->loop);

	if (ctx->eventcb != NULL) {
		ctx->notifying++;
		(*ctx->eventcb)(ctx, POMP_EVENT_MSG, conn, msg, ctx->userdata);
		ctx->notifying--;
	}

	return 0;
}

/**
 * Notify a raw buffer read.
 * @param ctx : context.
 * @param conn : connection on which the buffer has been received.
 * @param buf : buffer to notify.
 * @return 0 in case of success, negative errno value in case of error.
 */
int pomp_ctx_notify_raw_buf(struct pomp_ctx *ctx, struct pomp_conn *conn,
		struct pomp_buffer *buf)
{
	POMP_RETURN_ERR_IF_FAILED(ctx != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(ctx->rawcb != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(conn != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(buf != NULL, -EINVAL);
	POMP_LOOP_CHECK_OWNER(ctx->loop);

	ctx->notifying++;
	(*ctx->rawcb)(ctx, conn, buf, ctx->userdata);
	ctx->notifying--;
	return 0;
}

/**
 * Notify completion status of a send operation.
 * @param ctx : context.
 * @param conn : connection on which the send operation was done.
 * @param buf : buffer sent.
 * @param status : status of the send operation.
 * @return 0 in case of success, negative errno value in case of error.
 */
int pomp_ctx_notify_send(struct pomp_ctx *ctx, struct pomp_conn *conn,
		struct pomp_buffer *buf, uint32_t status)
{
	POMP_RETURN_ERR_IF_FAILED(ctx != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(conn != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(buf != NULL, -EINVAL);
	POMP_LOOP_CHECK_OWNER(ctx->loop);

	if (ctx->sendcb != NULL) {
		ctx->notifying++;
		(*ctx->sendcb)(ctx, conn, buf, status, NULL, ctx->userdata);
		ctx->notifying--;
	}
	return 0;
}

/**
 * Check if the send callback is set.
 * @param ctx : context.
 * @return 1 if the send callback is set else 0.
 */
int pomp_ctx_sendcb_is_set(struct pomp_ctx *ctx)
{
	POMP_RETURN_VAL_IF_FAILED(ctx != NULL, -EINVAL, 0);

	return (ctx->sendcb != NULL) ? 1 : 0;
}
