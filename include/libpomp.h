/**
 * @file libpomp.h
 *
 * @brief Printf Oriented Message Protocol.
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

#ifndef _LIBPOMP_H_
#define _LIBPOMP_H_

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Wrapper for gcc printf attribute */
#ifndef POMP_ATTRIBUTE_FORMAT_PRINTF
#  if defined(__GNUC__) && defined(__MINGW32__) && !defined(__clang__)
#    define POMP_ATTRIBUTE_FORMAT_PRINTF(_x, _y) \
		__attribute__((__format__(__gnu_printf__, _x, _y)))
#  elif defined(__GNUC__)
#    define POMP_ATTRIBUTE_FORMAT_PRINTF(_x, _y) \
		__attribute__((__format__(__printf__, _x, _y)))
#  else
#    define POMP_ATTRIBUTE_FORMAT_PRINTF(_x, _y)
#  endif
#endif /* !POMP_ATTRIBUTE_FORMAT_PRINTF */

/** Wrapper for gcc scanf attribute */
#ifndef POMP_ATTRIBUTE_FORMAT_SCANF
#  if defined(__GNUC__) && defined(__MINGW32__) && !defined(__clang__)
#    define POMP_ATTRIBUTE_FORMAT_SCANF(_x, _y) \
		__attribute__((__format__(__gnu_scanf__, _x, _y)))
#  elif defined(__GNUC__)
#    define POMP_ATTRIBUTE_FORMAT_SCANF(_x, _y) \
		__attribute__((__format__(__scanf__, _x, _y)))
#  else
#    define POMP_ATTRIBUTE_FORMAT_SCANF(_x, _y)
#  endif
#endif /* !POMP_ATTRIBUTE_FORMAT_SCANF */

/** To be used for all public API */
#ifdef POMP_API_EXPORTS
#  ifdef _WIN32
#    define POMP_API	__declspec(dllexport)
#  else /* !_WIN32 */
#    define POMP_API	__attribute__((visibility("default")))
#  endif /* !_WIN32 */
#else /* !POMP_API_EXPORTS */
#  define POMP_API
#endif /* !POMP_API_EXPORTS */

/* Forward declarations */
struct sockaddr;
struct pomp_ctx;
struct pomp_conn;
struct pomp_buffer;
struct pomp_msg;
struct pomp_loop;
struct pomp_timer;

/** Context event */
enum pomp_event {
	POMP_EVENT_CONNECTED = 0,	/**< Peer is connected */
	POMP_EVENT_DISCONNECTED,	/**< Peer is disconnected */
	POMP_EVENT_MSG,			/**< Message received from peer */
};

/**
 * Get the string description of a context event.
 * @param event : event to convert.
 * @return string description of the context event.
 */
POMP_API const char *pomp_event_str(enum pomp_event event);

/** Fd events */
enum pomp_fd_event {
	POMP_FD_EVENT_IN = 0x001,
	POMP_FD_EVENT_PRI = 0x002,
	POMP_FD_EVENT_OUT = 0x004,
	POMP_FD_EVENT_ERR = 0x008,
	POMP_FD_EVENT_HUP = 0x010,
};

/** Socket kind */
enum pomp_socket_kind {
	POMP_SOCKET_KIND_SERVER,	/**< Server socket */
	POMP_SOCKET_KIND_PEER,		/**< Peer (accepted) socket */
	POMP_SOCKET_KIND_CLIENT,	/**< Client socket */
	POMP_SOCKET_KIND_DGRAM,		/**< Dgram socket */
};

/**
 * Get the string description of a socket kind.
 * @param kind : socket kind to convert.
 * @return string description of the socket kind.
 */
POMP_API const char *pomp_socket_kind_str(enum pomp_socket_kind kind);

/** Send status flags */
enum pomp_send_status {
	POMP_SEND_STATUS_OK = 0x01,		/**< Send is OK */
	POMP_SEND_STATUS_ERROR = 0x03,		/**< Error during send */
	POMP_SEND_STATUS_ABORTED = 0x04,	/**< Send aborted */
	POMP_SEND_STATUS_QUEUE_EMPTY = 0x08,	/**< No more buffer in queue */
};

/** Peer credentials for local sockets */
struct pomp_cred {
	uint32_t	pid;	/**< PID of sending process */
	uint32_t	uid;	/**< UID of sending process */
	uint32_t	gid;	/**< GID of sending process */
};

/**
 * Context event callback prototype.
 * @param ctx : context.
 * @param event : event that occurred.
 * @param conn : connection on which the event occurred.
 * @param msg : received message when event is POMP_EVENT_MSG.
 * @param userdata : user data given in pomp_ctx_new.
 */
typedef void (*pomp_event_cb_t)(
		struct pomp_ctx *ctx,
		enum pomp_event event,
		struct pomp_conn *conn,
		const struct pomp_msg *msg,
		void *userdata);

/**
 * Context raw data reception callback prototype.
 * @param ctx : context.
 * @param conn : connection on which the event occurred.
 * @param buf : received data.
 * @param userdata : user data given in pomp_ctx_new.
 */
typedef void (*pomp_ctx_raw_cb_t)(
		struct pomp_ctx *ctx,
		struct pomp_conn *conn,
		struct pomp_buffer *buf,
		void *userdata);

/**
 * Context socket callback. If set, will be called after socket is created.
 * @param ctx : context.
 * @param fd : socket fd.
 * @param kind : socket kind.
 * @param userdata : user data given in pomp_ctx_new.
 */
typedef void (*pomp_socket_cb_t)(
		struct pomp_ctx *ctx,
		int fd,
		enum pomp_socket_kind kind,
		void *userdata);

/**
 * Send callback. If set, it is called to indicate that the given buffer has
 * been sent (or not).
 * @param ctx : context.
 * @param conn : connection on which the event occurred.
 * @param buf : buffer whose send status id notified. If in raw mode is it the
 * one given in send operation, otherwise it is the internal buffer associated
 * with the message sent.
 * @param status : set of flags (enum pomp_send_status) indicating the status.
 * one of OK, ERROR or ABORTED is always set. QUEUE_EMPTY indicates that there
 * is no more buffer queued internally.
 * @param cookie : NULL, reserved for future use.
 * @param userdata : user data given in pomp_ctx_new.
 *
 */
typedef void (*pomp_send_cb_t)(
		struct pomp_ctx *ctx,
		struct pomp_conn *conn,
		struct pomp_buffer *buf,
		uint32_t status,
		void *cookie,
		void *userdata);

/**
 * Fd event callback.
 * @param fd : triggered fd.
 * @param revents : event that occurred.
 * @param userdata : callback user data.
 */
typedef void (*pomp_fd_event_cb_t)(int fd, uint32_t revents, void *userdata);

/**
 * Timer callback
 * @param timer : timer.
 * @param userdata : callback user data.
 */
typedef void (*pomp_timer_cb_t) (struct pomp_timer *timer, void *userdata);

/**
 * Idle callback.
 * @param userdata : callback user data.
 */
typedef void (*pomp_idle_cb_t)(void *userdata);

/*
 * Context API.
 */

/**
 * Create a new context structure.
 * @param cb : function to be called when connection/disconnection/message
 * events occur.
 * @param userdata : user data to give in cb.
 * @return context structure or NULL in case of error.
 */
POMP_API struct pomp_ctx *pomp_ctx_new(pomp_event_cb_t cb, void *userdata);

/**
 * Create a new context structure in an existing loop.
 * @param cb : function to be called when connection/disconnection/message
 * events occur.
 * @param userdata : user data to give in cb.
 * @param loop: loop to use.
 * @return context structure or NULL in case of error.
 */
POMP_API struct pomp_ctx *pomp_ctx_new_with_loop(pomp_event_cb_t cb,
		void *userdata, struct pomp_loop *loop);

/**
 * Mark the context as raw. Internal message protocol serialization will not
 * be used, only raw data can be sent/received. Connection/Disconnection
 * events will be notified with the generic context event callback.
 * message API.
 * @param ctx : context.
 * @param cb : function to call when data has been received. The userdata
 * argument will be the same as the one set when creating the context.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_ctx_set_raw(struct pomp_ctx *ctx, pomp_ctx_raw_cb_t cb);

/**
 * Set the function to call when socket fd are created. This allows application
 * to configure socket before it is used.
 * @param ctx : context.
 * @param cb : function to call when socket are created. The userdata
 * argument will be the same as the one set when creating the context.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_ctx_set_socket_cb(struct pomp_ctx *ctx, pomp_socket_cb_t cb);

/**
 * Set the function to call when send operations are completed. This allows
 * application to be notified of actual completion in case operations are
 * internally queued.
 * @param ctx : context.
 * @param cb : function to call when send operations are completed. The userdata
 * argument will be the same as the one set when creating the context.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_ctx_set_send_cb(struct pomp_ctx *ctx, pomp_send_cb_t cb);

/**
 * Setup TCP keepalive. Settings will be applied to all future TCP connections.
 * Current connections (if any) will not be affected.
 * @param ctx : context.
 * @param enable : 1 to enable, 0, to disable.
 * @param idle : start keepalives after this period (in seconds).
 * @param interval : Interval between keepalives (in seconds).
 * @param count : number of keepalives before death.
 * @return 0 in case of success, negative errno value in case of error.
 *
 * @remarks Default values if nothing else is specified is (1, 5, 1, 2).
 */
POMP_API int pomp_ctx_setup_keepalive(struct pomp_ctx *ctx, int enable,
		int idle, int interval, int count);

/**
 * Destroy a context.
 * @param ctx : context.
 * @return 0 in case of success, negative errno value in case of error.
 * If the client or server is still running, -EBUSY is returned.
 */
POMP_API int pomp_ctx_destroy(struct pomp_ctx *ctx);

/**
 * Start a server.
 * @param ctx : context.
 * @param addr : local address to listen on.
 * @param addrlen : local address size.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_ctx_listen(struct pomp_ctx *ctx,
		const struct sockaddr *addr, uint32_t addrlen);

/**
 * Start a client.
 * If connection can not be completed immediately, it will try again later
 * automatically. Call pomp_ctx_stop to disconnect and stop everything.
 * @param ctx : context.
 * @param addr : remote address to connect to.
 * @param addrlen : remote address size.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_ctx_connect(struct pomp_ctx *ctx,
		const struct sockaddr *addr, uint32_t addrlen);

/**
 * Bind a connection-less context (inet-udp).
 * @param ctx : context.
 * @param addr : address to bind to.
 * @param addrlen : address size.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_ctx_bind(struct pomp_ctx *ctx,
		const struct sockaddr *addr, uint32_t addrlen);

/**
 * Stop the context. It will disconnects all peers (with notification). The
 * context structure itself is not freed. It can be used again with listen or
 * connect.
 * @param ctx : context.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_ctx_stop(struct pomp_ctx *ctx);

/**
 * Get the internal loop structure of the context.
 * @param ctx : context.
 * @return loop structure or NULL in case of error.
 */
POMP_API struct pomp_loop *pomp_ctx_get_loop(struct pomp_ctx *ctx);

/**
 * Get the fd/event of the loop associated with the context.
 *
 * This fd/event shall be put in the user main loop (select, poll, epoll, glib,
 * win32...) and monitored for input events. When it becomes readable, the
 * function pomp_ctx_process_fd shall be called to dispatch internal events.
 *
 * @param ctx : context.
 * @return file descriptor (or event handle for win32), negative errno value
 * in case of error.
 *
 * @remarks see pomp_loop_getfd for more information.
 */
POMP_API intptr_t pomp_ctx_get_fd(const struct pomp_ctx *ctx);

/**
 * Function to be called when the loop of the context is signaled.
 * @param ctx : context.
 * @return 0 in case of success, negative errno value in case of error.
 * @remarks this is equivalent to calling pomp_ctx_wait_and_process with a
 * timeout of 0 (no wait).
 */
POMP_API int pomp_ctx_process_fd(struct pomp_ctx *ctx);

/**
 * Wait for events to occur in the context and process them.
 * @param ctx : context.
 * @param timeout : timeout of wait (in ms) or -1 for infinite wait.
 * @return 0 in case of success, -ETIMEDOUT if timeout occurred,
 * negative errno value in case of error.
 */
POMP_API int pomp_ctx_wait_and_process(struct pomp_ctx *ctx, int timeout);

/**
 * Wakeup a context from a wait in pomp_ctx_wait_and_process.
 * @param ctx : context.
 * @return 0 in case of success, negative errno value in case of error.
 *
 * @remarks: this function is safe to call from another thread that the one
 * associated normally with the context. It is also safe to call it from a
 * signal handler. However caller must ensure that the given context will be
 * valid for the complete duration of the call.
 */
POMP_API int pomp_ctx_wakeup(struct pomp_ctx *ctx);

/**
 * Get next connection structure for a server context.
 * @param ctx : context (shall be a server one).
 * @param prev : previous connection structure or NULL to get the first one.
 * @return connection structure or NULL if there is no more connection.
 */
POMP_API struct pomp_conn *pomp_ctx_get_next_conn(const struct pomp_ctx *ctx,
		const struct pomp_conn *prev);

/**
 * Get the connection structure for a client context.
 * @param ctx : context (shall be a client one).
 * @return connection structure or NULL if there is no connection with server.
 */
POMP_API struct pomp_conn *pomp_ctx_get_conn(const struct pomp_ctx *ctx);

/**
 * Get context local address (for server or udp context started with listen or
 * bind ).
 * @param ctx : context (shall be a server or udp one).
 * @param addrlen : returned address size.
 * @return local address or NULL in case of error.
 */
POMP_API const struct sockaddr *pomp_ctx_get_local_addr(struct pomp_ctx *ctx,
		uint32_t *addrlen);

/**
 * Send a message to a context.
 * For server it will broadcast to all connected clients. If there is no
 * connection, the message is lost and no error is returned.
 * For client, if there is no connection, -ENOTCONN is returned.
 * @param ctx : context.
 * @param msg : message to send.
 * @return 0 in case of success, negative errno value in case of error.
 *
 * @remarks the operation will be queued automatically for later processing
 * in case the underlying socket buffer is full. Use 'pomp_ctx_set_socket_cb'
 * to have more information about completion.
 */
POMP_API int pomp_ctx_send_msg(struct pomp_ctx *ctx,
		const struct pomp_msg *msg);

/**
 * Send a message on dgram context to a remote address.
 * @param ctx : context.
 * @param msg : message to send.
 * @param addr : destination address.
 * @param addrlen : address size.
 * @return 0 in case of success, negative errno value in case of error.
 *
 * @remarks the operation will be queued automatically for later processing
 * in case the underlying socket buffer is full. Use 'pomp_ctx_set_socket_cb'
 * to have more information about completion.
 */
POMP_API int pomp_ctx_send_msg_to(struct pomp_ctx *ctx,
		const struct pomp_msg *msg,
		const struct sockaddr *addr, uint32_t addrlen);

/**
 * Format and send a message to a context.
 * For server it will broadcast to all connected clients. If there is no
 * connection, no message is sent and no error is returned.
 * For client, if there is no connection, -ENOTCONN is returned.
 * @param ctx : context.
 * @param msgid : message id.
 * @param fmt : format string. Can be NULL if no arguments given.
 * @param ... : message arguments.
 * @return 0 in case of success, negative errno value in case of error.
 *
 * @remarks the operation will be queued automatically for later processing
 * in case the underlying socket buffer is full. Use 'pomp_ctx_set_socket_cb'
 * to have more information about completion.
 */
POMP_API int pomp_ctx_send(struct pomp_ctx *ctx, uint32_t msgid,
		const char *fmt, ...) POMP_ATTRIBUTE_FORMAT_PRINTF(3, 4);


/**
 * Format and send a message to a context.
 * For server it will broadcast to all connected clients. If there is no
 * connection, the message is lost and no error is returned.
 * For client, if there is no connection, -ENOTCONN is returned.
 * @param ctx : context.
 * @param msgid : message id.
 * @param fmt : format string. Can be NULL if no arguments given.
 * @param args : message arguments.
 * @return 0 in case of success, negative errno value in case of error.
 *
 * @remarks the operation will be queued automatically for later processing
 * in case the underlying socket buffer is full. Use 'pomp_ctx_set_socket_cb'
 * to have more information about completion.
 */
POMP_API int pomp_ctx_sendv(struct pomp_ctx *ctx, uint32_t msgid,
		const char *fmt, va_list args);

/**
 * Send a buffer to a raw context.
 * For server it will broadcast to all connected clients. If there is no
 * connection, no buffer is sent and no error is returned.
 * For client, if there is no connection, -ENOTCONN is returned.
 * @param ctx : context.
 * @param buf : buffer to send.
 * @return 0 in case of success, negative errno value in case of error.
 *
 * @remarks the operation will be queued automatically for later processing
 * in case the underlying socket buffer is full. Use 'pomp_ctx_set_socket_cb'
 * to have more information about completion.
 */
POMP_API int pomp_ctx_send_raw_buf(struct pomp_ctx *ctx,
		struct pomp_buffer *buf);

/**
 * Send a buffer on dgram raw context to a remote address.
 * @param ctx : context.
 * @param buff : buffer to send.
 * @param addr : destination address.
 * @param addrlen : address size.
 * @return 0 in case of success, negative errno value in case of error.
 *
 * @remarks the operation will be queued automatically for later processing
 * in case the underlying socket buffer is full. Use 'pomp_ctx_set_socket_cb'
 * to have more information about completion.
 */
POMP_API int pomp_ctx_send_raw_buf_to(struct pomp_ctx *ctx,
		struct pomp_buffer *buf,
		const struct sockaddr *addr, uint32_t addrlen);

/*
 * Connection API.
 */

/**
 * Force disconnection of an established connection.
 * @param conn : connection
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_conn_disconnect(struct pomp_conn *conn);

/**
 * Get connection local address.
 * @param conn : connection
 * @param addrlen : returned address size.
 * @return local address or NULL in case of error.
 */
POMP_API const struct sockaddr *pomp_conn_get_local_addr(struct pomp_conn *conn,
		uint32_t *addrlen);

/**
 * Get connection remote peer address.
 * @param conn : connection
 * @param addrlen : returned address size.
 * @return remote peer address or NULL in case of error.
 */
POMP_API const struct sockaddr *pomp_conn_get_peer_addr(struct pomp_conn *conn,
		uint32_t *addrlen);

/**
 * Get connection remote peer credentials for local sockets.
 * @param conn : connection
 * @return connection remote peer credentials or NULL if not a local socket.
 */
POMP_API const struct pomp_cred *pomp_conn_get_peer_cred(
		struct pomp_conn *conn);

/**
 * Get file descriptor associated with the connection.
 * @param conn : connection.
 * @return fd of the connection or -EINVAL in case of error.
 */
POMP_API int pomp_conn_get_fd(struct pomp_conn *conn);

/**
 * Suspend read operation on connection.
 * @param conn : connection.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_conn_suspend_read(struct pomp_conn *conn);

/**
 * Resume read operation on connection.
 * @param conn : connection.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_conn_resume_read(struct pomp_conn *conn);

/**
 * Send a message to the peer of the connection.
 * @param conn : connection.
 * @param msg : message to send.
 * @return 0 in case of success, negative errno value in case of error.
 *
 * @remarks the operation will be queued automatically for later processing
 * in case the underlying socket buffer is full. Use 'pomp_ctx_set_socket_cb'
 * to have more information about completion.
 */
POMP_API int pomp_conn_send_msg(struct pomp_conn *conn,
		const struct pomp_msg *msg);

/**
 * Format and send a message to the peer of the connection.
 * @param conn : connection.
 * @param msgid : message id.
 * @param fmt : format string. Can be NULL if no arguments given.
 * @param ... : message arguments.
 * @return 0 in case of success, negative errno value in case of error.
 *
 * @remarks the operation will be queued automatically for later processing
 * in case the underlying socket buffer is full. Use 'pomp_ctx_set_socket_cb'
 * to have more information about completion.
 */
POMP_API int pomp_conn_send(struct pomp_conn *conn, uint32_t msgid,
		const char *fmt, ...) POMP_ATTRIBUTE_FORMAT_PRINTF(3, 4);

/**
 * Format and send a message to the peer of the connection.
 * @param conn : connection.
 * @param msgid : message id.
 * @param fmt : format string. Can be NULL if no arguments given.
 * @param args : message arguments.
 * @return 0 in case of success, negative errno value in case of error.
 *
 * @remarks the operation will be queued automatically for later processing
 * in case the underlying socket buffer is full. Use 'pomp_ctx_set_socket_cb'
 * to have more information about completion.
 */
POMP_API int pomp_conn_sendv(struct pomp_conn *conn, uint32_t msgid,
		const char *fmt, va_list args);

/**
 * Send a buffer to the peer of the raw connection.
 * @param conn : connection.
 * @param buf : buffer to send.
 * @return 0 in case of success, negative errno value in case of error.
 *
 * @remarks the operation will be queued automatically for later processing
 * in case the underlying socket buffer is full. Use 'pomp_ctx_set_socket_cb'
 * to have more information about completion.
 */
POMP_API int pomp_conn_send_raw_buf(struct pomp_conn *conn,
		struct pomp_buffer *buf);

/*
 * Buffer API.
 */

/**
 * Allocate a new buffer.
 * @param capacity : initial capacity of the buffer.
 * @return new buffer with initial ref count at 1 or NULL in case of error.
 */
POMP_API struct pomp_buffer *pomp_buffer_new(size_t capacity);

/**
 * Create a new buffer with content copied from another buffer.
 * @param buf : buffer to copy.
 * @return new buffer with initial ref count at 1 and internal data copied from
 * given buffer or NULL in case of error.
 */
POMP_API struct pomp_buffer *pomp_buffer_new_copy(
		const struct pomp_buffer *buf);

/**
 * Create a new buffer with content copied from given data. Internal length and
 * capacity will be set to given data length.
 * @param data : data to copy.
 * @param len : lebth to copy.
 * @return new buffer with initial ref count at 1 and internal data copied from
 * given buffer or NULL in case of error.
 */
POMP_API struct pomp_buffer *pomp_buffer_new_with_data(
		const void *data, size_t len);

/**
 * Same as pomp_buffer_new but retreived internal data pointer at the same time.
 * @param capacity : initial capacity of the buffer.
 * @param data : data of buffer (optional can be NULL).
 * @return new buffer with initial ref count at 1 or NULL in case of error.
 */
POMP_API struct pomp_buffer *pomp_buffer_new_get_data(
		size_t capacity, void **data);

/**
 * Increase ref count of buffer.
 * @param buf : buffer.
 */
POMP_API void pomp_buffer_ref(struct pomp_buffer *buf);

/**
 * Decrease ref count of buffer. When it reaches 0, internal data as well as
 * buffer structure itself is freed.
 * @param buf : buffer.
 */
POMP_API void pomp_buffer_unref(struct pomp_buffer *buf);

/**
 * Determine if the buffer is shared
 * @param buf : buffer.
 * @return 1 if the buffer is shared (ref count greater than 1), 0 otherwise.
 */
POMP_API int pomp_buffer_is_shared(struct pomp_buffer *buf);

/**
 * Set the capacity of the buffer.
 * @param buf : buffer.
 * @param capacity : new capacity of buffer (shall be greater than used length).
 * @return 0 in case of success, negative errno value in case of error.
 * -EPERM is returned if the buffer is shared (ref count is greater than 1).
 */
POMP_API int pomp_buffer_set_capacity(struct pomp_buffer *buf, size_t capacity);

/**
 * Make sure internal data has enough room for the given size.
 * @param buf : buffer.
 * @param capacity : new capacity of buffer.
 * @return 0 in case of success, negative errno value in case of error.
 * -EPERM is returned if the buffer is shared (ref count is greater than 1).
 *
 * @remarks : internally the size will be aligned to POMP_BUFFER_ALLOC_STEP.
 */
POMP_API int pomp_buffer_ensure_capacity(struct pomp_buffer *buf,
		size_t capacity);

/**
 * Set the used length of the buffer.
 * @param buf : buffer.
 * @param len : used length of data (shall be lesser than allocated size).
 * @return 0 in case of success, negative errno value in case of error.
 * -EPERM is returned if the buffer is shared (ref count is greater than 1) or
 * readonly.
 */
POMP_API int pomp_buffer_set_len(struct pomp_buffer *buf, size_t len);

/**
 * Get internal buffer data for read/write.
 * @param buf : buffer.
 * @param data : data of buffer (optional can be NULL).
 * @param len : used length of buffer (optional can be NULL).
 * @param capacity : capacity of buffer (optional can be NULL).
 * @return 0 in case of success, negative errno value in case of error.
 * -EPERM is returned if the buffer is shared.
 */
POMP_API int pomp_buffer_get_data(struct pomp_buffer *buf,
		void **data, size_t *len, size_t *capacity);

/**
 * Get internal buffer data for read-only.
 * @param buf : buffer
 * @param cdata : data of buffer (optional can be NULL).
 * @param len : used length of buffer (optional can be NULL).
 * @param capacity : capacity of buffer (optional can be NULL).
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_buffer_get_cdata(struct pomp_buffer *buf,
		const void **cdata, size_t *len, size_t *size);

/**
 * Append data to the buffer.
 * @param buf : buffer.
 * @param data : data to append.
 * @param len : length of the data to append.
 * @return 0 in case of success, negative errno value in case of error.
 * -EPERM is returned if the buffer is shared.
 *
 * @remarks Can re-allocate the internal data buffer and invalidate the
 * data/cdata retrieved before.
 */
POMP_API int pomp_buffer_append_data(struct pomp_buffer *buf,
		const void *data, size_t len);

/**
 * Write data to the buffer at the given position.
 * @param buf : buffer.
 * @param pos : position in the buffer (will be updated after success)
 * @param data : data to write.
 * @param len : length of the data to write.
 * @return 0 in case of success, negative errno value in case of error.
 * -EPERM is returned if the buffer is shared.
 *
 * @remarks Can re-allocate the internal data buffer and invalidate the
 * data/cdata retrieved before.
 */
POMP_API int pomp_buffer_write(struct pomp_buffer *buf, size_t *pos,
		const void *data, size_t len);

/**
 * Read data from buffer at the given position.
 * @param buf : buffer.
 * @param pos : read position. It will be updated in case of success.
 * @param data : pointer to data to read.
 * @param len : number of bytes to read.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_buffer_read(const struct pomp_buffer *buf, size_t *pos,
		void *p, size_t n);

/**
 * Read data from buffer without copy.
 * @param buf : buffer.
 * @param pos : read position. It will be updated in case of success.
 * @param cdata : will receive pointer to data inside buffer. It is valid as
 * long as the buffer is valid and no write or resize operation is performed.
 * @param len : number of bytes to read.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_buffer_cread(const struct pomp_buffer *buf, size_t *pos,
		const void **cdata, size_t len);

/*
 * Message API.
 */

/**
 * Create a new message structure.
 * Message is initially empty.
 * @return new message structure or NULL in case of error.
 */
POMP_API struct pomp_msg *pomp_msg_new(void);

/**
 * Create a new message structure.
 * Message will be a copy of given message (with internal buffer copied as
 * well, not just shared).
 * @param msg : message to copy.
 * @return new message structure or NULL in case of error.
 *
 * @remarks : if the message contains file descriptors, they are duplicated in
 * the new message.
 */
POMP_API struct pomp_msg *pomp_msg_new_copy(const struct pomp_msg *msg);

/**
 * Create a new message structure from a buufer with data.
 * @param buf : buffer with message content (header + playload).
 * @return new message structure or NULL in case of error.
 */
POMP_API struct pomp_msg *pomp_msg_new_with_buffer(struct pomp_buffer *buf);

/**
 * Destroy a message.
 * @param msg : message.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_msg_destroy(struct pomp_msg *msg);

/**
 * Get the id of the message.
 * @param msg : message.
 * @return id of the message or 0 in case of error.
 */
POMP_API uint32_t pomp_msg_get_id(const struct pomp_msg *msg);

/**
 * Get the internal buffer of the message.
 * @param msg : message.
 * @return internal buffer of the message or NULL in case of error.
 *
 * @remarks this function is useful when only the serialization of the library
 * is needed, not the transport part. This way one can write the data to another
 * transport layer.
 */
POMP_API struct pomp_buffer *pomp_msg_get_buffer(const struct pomp_msg *msg);

/**
 * Write and encode a message.
 * @param msg : message.
 * @param msgid : message id.
 * @param fmt : format string. Can be NULL if no arguments given.
 * @param ... : message arguments.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_msg_write(struct pomp_msg *msg, uint32_t msgid,
		const char *fmt, ...) POMP_ATTRIBUTE_FORMAT_PRINTF(3, 4);

/**
 * Write and encode a message.
 * @param msg : message.
 * @param msgid : message id.
 * @param fmt : format string. Can be NULL if no arguments given.
 * @param args : message arguments.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_msg_writev(struct pomp_msg *msg, uint32_t msgid,
		const char *fmt, va_list args);

/**
 * Write and encode a message.
 * @param msg : message.
 * @param msgid : message id.
 * @param fmt : format string. Can be NULL if no arguments given.
 * @param argc : number of arguments.
 * @param argv : array of arguments as strings. Each string will be converted
 *               according to its real type specified in format.
 * @return 0 in case of success, negative errno value in case of error.
 *
 * @remarks this is mainly used by the command line tool.
 */
POMP_API int pomp_msg_write_argv(struct pomp_msg *msg, uint32_t msgid,
		const char *fmt, int argc, const char * const *argv);

/**
 * Read and decode a message.
 * @param msg : message.
 * @param fmt : format string. Can be NULL if no arguments given.
 * @param ... : message arguments.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_msg_read(const struct pomp_msg *msg,
		const char *fmt, ...) POMP_ATTRIBUTE_FORMAT_SCANF(2, 3);

/**
 * Read and decode a message.
 * @param msg : message.
 * @param fmt : format string. Can be NULL if no arguments given.
 * @param args : message arguments.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_msg_readv(const struct pomp_msg *msg,
		const char *fmt, va_list args);

/**
 * Dump a message in a human readable form.
 * @param msg : message.
 * @param dst : destination buffer.
 * @param maxdst : max length of destination buffer.
 * @return 0 in case of success, negative errno value in case of error.

 * @remarks if the buffer is too small, it will be ended with ellipsis '...' if
 * possible. It will also always be null terminated (unless maxlen is 0)
 */
POMP_API int pomp_msg_dump(const struct pomp_msg *msg,
		char *dst, uint32_t maxdst);

/**
 * Dump a message in a human readable form. Similar to pomp_msg_dump() but
 * allocates the output buffer dynamically.
 * Dump a message in a human readable form.
 * @param msg : message.
 * @param dst : destination buffer, which will be allocated to a suitable size
 * dynamically. Must be freed with free(), after usage.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_msg_adump(const struct pomp_msg *msg, char **dst);

/*
 * Loop API.
 */

/**
 * Create a new loop structure.
 * @return loop structure or NULL in case of error.
 */
POMP_API struct pomp_loop *pomp_loop_new(void);

/**
 * Destroy a loop.
 * @param loop : loop to destroy.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_loop_destroy(struct pomp_loop *loop);

/**
 * Register a new fd in loop.
 * @param loop : loop.
 * @param fd : fd to register.
 * @param events : events to monitor. @see pomp_fd_event.
 * @param cb : callback for notifications.
 * @param userdata user data for callback.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_loop_add(struct pomp_loop *loop, int fd, uint32_t events,
		pomp_fd_event_cb_t cb, void *userdata);

/**
 * Modify the set of events to monitor for a registered fd.
 * @param loop : loop.
 * @param fd : fd to modify.
 * @param events : new events to monitor. @see pomp_fd_event.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_loop_update(struct pomp_loop *loop, int fd, uint32_t events);

/**
 * Modify the set of events to monitor for a registered fd.
 * @param loop : loop.
 * @param fd : fd to modify.
 * @param events_to_add : events to add. @see pomp_fd_event.
 * @param events_to_remove : events to remove. @see pomp_fd_event.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_loop_update2(struct pomp_loop *loop, int fd,
		uint32_t events_to_add, uint32_t events_to_remove);

/**
 * Unregister a fd from the loop
 * @param loop : loop.
 * @param fd : fd to unregister.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_loop_remove(struct pomp_loop *loop, int fd);

/**
 * Check if fd has been added in loop.
 * @param loop : loop.
 * @param fd : fd to check.
 * @return 1 if fd is in loop 0 otherwise.
 */
POMP_API int pomp_loop_has_fd(struct pomp_loop *loop, int fd);

/**
 * Get a file descriptor (or event handle for win32) that can be monitored for
 * reading (or waited for) when there is some activity in the loop.
 *
 * This fd/event shall be put in the user main loop (select, poll, epoll, glib,
 * win32...) and monitored for input events. When it becomes readable, the
 * function pomp_loop_wait_and_process shall be called to dispatch internal
 * events.
 *
 * @param loop : loop.
 * @return file descriptor (or event handle for win32), negative errno value
 * in case of error.
 *
 * @remarks for poll/win32 implementation this will create an internal worker
 * thread that will monitor all registered fd/handles and signal the single
 * fd/event returned by this function.
 */
POMP_API intptr_t pomp_loop_get_fd(struct pomp_loop *loop);

/**
 * Function to be called when the loop is signaled for readiness.
 * @param loop : loop.
 * @return 0 in case of success, negative errno value in case of error.
 * @remarks this is equivalent to calling pomp_loop_wait_and_process with a
 * timeout of 0 (no wait).
 */
POMP_API int pomp_loop_process_fd(struct pomp_loop *loop);

/**
 * Wait for events to occur in loop and process them.
 * @param loop : loop.
 * @param timeout : timeout of wait (in ms) or -1 for infinite wait.
 * @return 0 in case of success, -ETIMEDOUT if timeout occurred,
 * negative errno value in case of error.
 */
POMP_API int pomp_loop_wait_and_process(struct pomp_loop *loop, int timeout);

/**
 * Wakeup a loop from a wait in pomp_loop_wait_and_process.
 * @param loop : loop.
 * @return 0 in case of success, negative errno value in case of error.
 *
 * @remarks: this function is safe to call from another thread that the one
 * associated normally with the loop. It is also safe to call it from a
 * signal handler. However caller must ensure that the given context will be
 * valid for the complete duration of the call.
 */
POMP_API int pomp_loop_wakeup(struct pomp_loop *loop);

/**
 * Register a function to be called when loop is idle, i.e. there is no event
 * to be processed. The registered function will be called only once and in
 * the order they are registered.
 * @param loop : loop.
 * @param cb : callback to call.
 * @param userdata : user data for callback.
 * @return 0 in case of success, negative errno value in case of error. In
 * particular it is an error to register an idle function while currently
 * registered function are being called (-EPERM will be returned).
 *
 * @remarks: this function is useful to register cleanup functions when called
 * by an fd event callback for example.
 */
POMP_API int pomp_loop_idle_add(struct pomp_loop *loop, pomp_idle_cb_t cb,
		void *userdata);

/**
 * Unregister a function registered with pomp_loop_idle_add.
 * @param loop : loop.
 * @param cb : callback given in pomp_loop_idle_add.
 * @param userdata : user data given in pomp_loop_idle_add.
 * @return 0 in case of success, negative errno value in case of error.
 *
 * @remarks: if nothing match the given criteria, no error is returned.
 */
POMP_API int pomp_loop_idle_remove(struct pomp_loop *loop, pomp_idle_cb_t cb,
		void *userdata);

/*
 * Timer API.
 */

/**
 * Create a new timer.
 * @param loop : fd loop to use for notifications.
 * @param cb : callback to use for notifications.
 * @param userdata : user data for callback.
 * @return new timer or NULL in case of error.
 */
POMP_API struct pomp_timer *pomp_timer_new(struct pomp_loop *loop,
		pomp_timer_cb_t cb, void *userdata);

/**
 * Destroy a timer.
 * @param timer : timer to destroy.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_timer_destroy(struct pomp_timer *timer);

/**
 * Set a one shot timer.
 * @param timer : timer to set.
 * @param delay : expiration delay in milliseconds.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_timer_set(struct pomp_timer *timer, uint32_t delay);

/**
 * Set a periodic timer.
 * @param timer : timer to set.
 * @param delay : initial expiration delay in milliseconds.
 * @param period : period in milliseconds.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_timer_set_periodic(struct pomp_timer *timer, uint32_t delay,
		uint32_t period);

/**
 * Clear a timer.
 * @param timer : timer to clear.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_timer_clear(struct pomp_timer *timer);

/*
 * Address string parsing/formatting utilities.
 */

/**
 * Parse a socket address given as a string and convert it to sockaddr.
 * @param buf: input string.
 * @param addr: destination structure.
 * @param addrlen: maximum size of destination structure as input, real size
 * converted as output. Should be at least sizeof(struct sockaddr_storage)
 * @return 0 in case of success, negative errno value in case of error.
 *
 * Format of string is:
 * - inet:<host>:<port>: ipv4 address with host name and port.
 * - inet6:<host>:<port>: ipv6 address with host name and port
 * - unix:<pathname>: unix local address with file system name.
 * - unix:@<name>: unix local address with abstract name.
 */
POMP_API int pomp_addr_parse(const char *buf, struct sockaddr *addr,
		uint32_t *addrlen);

/**
 * Format a socket address into a string.
 * @param buf: destination buffer
 * @param buflen: maximum size of destination buffer.
 * @param addr: address to format.
 * @param addrlen: size of address.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_addr_format(char *buf, uint32_t buflen,
		const struct sockaddr *addr, uint32_t addrlen);

/**
 * Determine if a socket address is a unix local one.
 * @param addr: address to check.
 * @param addrlen: size of address.
 * @return 1 if socket is unix local, 0 otherwise.
 */
POMP_API int pomp_addr_is_unix(const struct sockaddr *addr, uint32_t addrlen);

/*
 * Advanced API.
 *
 * Basic API is sufficient for normal usage. Advanced API offers better
 * tuning of encoding/decoding.
 */

/* Forward declarations */
struct pomp_encoder;
struct pomp_decoder;

/*
 * message API (Advanced).
 */

/**
 * Initialize a message object before starting to encode it.
 * @param msg : message.
 * @param msgid : message id.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_msg_init(struct pomp_msg *msg, uint32_t msgid);

/**
 * Finish message encoding by writing the header. It shall be called after
 * encoding is done and before sending it. Any write operation on the message
 * will return -EPERM after this function is called.
 * @param msg : message.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_msg_finish(struct pomp_msg *msg);

/**
 * Clear message object. It only free the internal data, not the message itself.
 * It shall be called before pomp_msg_init can be called again after a previous
 * encoding.
 * @param msg : message.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_msg_clear(struct pomp_msg *msg);

/*
 * Encoder API (Advanced).
 */

/**
 * Create a new encoder object.
 * @return new encoder object or NULL in case of error.
 */
POMP_API struct pomp_encoder *pomp_encoder_new(void);

/**
 * Destroy an encoder object.
 * @param enc : encoder.
 * @return 0 in case of success, negative errno value in case of error.
 *
 * @remarks the associated message is NOT destroyed.
 */
POMP_API int pomp_encoder_destroy(struct pomp_encoder *enc);

/**
 * Initialize an encoder object before starting to encode a message.
 * @param enc : encoder.
 * @param msg : message to encode.
 * @return 0 in case of success, negative errno value in case of error.
 *
 * @remarks the message ownership is not transferred, it will NOT be destroyed
 * when encoder object is destroyed.
 */
POMP_API int pomp_encoder_init(struct pomp_encoder *enc, struct pomp_msg *msg);

/**
 * Clear encoder object.
 * @param enc : encoder.
 * @return 0 in case of success, negative errno value in case of error.
 *
 * @remarks the associated message is NOT destroyed.
 */
POMP_API int pomp_encoder_clear(struct pomp_encoder *enc);

/**
 * Encode arguments according to given format string.
 * @param enc : encoder.
 * @param fmt : format string. Can be NULL if no arguments given.
 * @param ... : arguments.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_encoder_write(struct pomp_encoder *enc,
		const char *fmt, ...) POMP_ATTRIBUTE_FORMAT_PRINTF(2, 3);

/**
 * Encode arguments according to given format string.
 * @param enc : encoder.
 * @param fmt : format string. Can be NULL if no arguments given.
 * @param args : arguments.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_encoder_writev(struct pomp_encoder *enc,
		const char *fmt, va_list args);

/**
 * Encode arguments according to given format string.
 * @param enc : encoder.
 * @param fmt : format string. Can be NULL if no arguments given.
 * @param argc : number of arguments.
 * @param argv : array of arguments as strings. Each string will be converted
 *               according to its real type specified in format.
 * @return 0 in case of success, negative errno value in case of error.
 *
 * @remarks this is mainly used by the command line tool.
 */
POMP_API int pomp_encoder_write_argv(struct pomp_encoder *enc,
		const char *fmt, int argc, const char * const *argv);

/**
 * Encode a 8-bit signed integer.
 * @param enc : encoder.
 * @param v : value to encode.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_encoder_write_i8(struct pomp_encoder *enc, int8_t v);

/**
 * Encode a 8-bit unsigned integer.
 * @param enc : encoder.
 * @param v : value to encode.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_encoder_write_u8(struct pomp_encoder *enc, uint8_t v);

/**
 * Encode a 16-bit signed integer.
 * @param enc : encoder.
 * @param v : value to encode.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_encoder_write_i16(struct pomp_encoder *enc, int16_t v);

/**
 * Encode a 16-bit unsigned integer.
 * @param enc : encoder.
 * @param v : value to encode.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_encoder_write_u16(struct pomp_encoder *enc, uint16_t v);

/**
 * Encode a 32-bit signed integer.
 * @param enc : encoder.
 * @param v : value to encode.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_encoder_write_i32(struct pomp_encoder *enc, int32_t v);

/**
 * Encode a 32-bit unsigned integer.
 * @param enc : encoder.
 * @param v : value to encode.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_encoder_write_u32(struct pomp_encoder *enc, uint32_t v);

/**
 * Encode a 64-bit signed integer.
 * @param enc : encoder.
 * @param v : value to encode.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_encoder_write_i64(struct pomp_encoder *enc, int64_t v);

/**
 * Encode a 64-bit unsigned integer.
 * @param enc : encoder.
 * @param v : value to encode.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_encoder_write_u64(struct pomp_encoder *enc, uint64_t v);

/**
 * Encode a string.
 * @param enc : encoder.
 * @param v : string to encode.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_encoder_write_str(struct pomp_encoder *enc, const char *v);

/**
 * Encode a buffer.
 * @param enc : encoder.
 * @param v : buffer to encode.
 * @param n : buffer size.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_encoder_write_buf(struct pomp_encoder *enc, const void *v,
		uint32_t n);

/**
 * Encode a 32-bit floating point.
 * @param enc : encoder.
 * @param v : value to encode.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_encoder_write_f32(struct pomp_encoder *enc, float v);

/**
 * Encode a 64-bit floating point.
 * @param enc : encoder.
 * @param v : value to encode.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_encoder_write_f64(struct pomp_encoder *enc, double v);

/**
 * Encode a file descriptor. It will be internally duplicated and close when
 * associated message is released.
 * @param enc : encoder.
 * @param v : value to encode.
 * @return 0 in case of success, negative errno value in case of error.
 *
 * @remarks : the encoded message will only be able to be exchanged over
 * unix local socket.
 */
POMP_API int pomp_encoder_write_fd(struct pomp_encoder *enc, int v);

/*
 * Decoder API (Advanced).
 */

/**
 * Create a new decoder object.
 * @return new decoder object or NULL in case of error.
 */
POMP_API struct pomp_decoder *pomp_decoder_new(void);

/**
 * Destroy a decoder object.
 * @param dec : decoder.
 * @return 0 in case of success, negative errno value in case of error.
 *
 * @remarks the associated message is NOT destroyed.
 */
POMP_API int pomp_decoder_destroy(struct pomp_decoder *dec);

/**
 * Initialize a decoder object before starting to decode a message.
 * @param dec : encoder.
 * @param msg : message to decode.
 * @return 0 in case of success, negative errno value in case of error.
 *
 * @remarks the message ownership is not transferred, it will NOT be destroyed
 * when decoder object is destroyed.
 */
POMP_API int pomp_decoder_init(struct pomp_decoder *dec,
		const struct pomp_msg *msg);

/**
 * Clear decoder object.
 * @param dec : encoder.
 * @return 0 in case of success, negative errno value in case of error.
 *
 * @remarks the associated message is NOT destroyed.
 */
POMP_API int pomp_decoder_clear(struct pomp_decoder *dec);

/**
 * Decode arguments according to given format string.
 * @param dec : decoder.
 * @param fmt : format string. Can be NULL if no arguments given.
 * @param ... : arguments.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_decoder_read(struct pomp_decoder *dec,
		const char *fmt, ...) POMP_ATTRIBUTE_FORMAT_SCANF(2, 3);

/**
 * Decode arguments according to given format string.
 * @param dec : decoder.
 * @param fmt : format string. Can be NULL if no arguments given.
 * @param args : arguments.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_decoder_readv(struct pomp_decoder *dec,
		const char *fmt, va_list args);

/**
 * Dump arguments in a human readable form.
 * @param dec : decoder.
 * @param dst : destination buffer.
 * @param maxdst : max length of destination buffer.
 * @return 0 in case of success, negative errno value in case of error.

 * @remarks if the buffer is too small, it will be ended with ellipsis '...' if
 * possible. It will also always be null terminated (unless maxlen is 0)
 */
POMP_API int pomp_decoder_dump(struct pomp_decoder *dec,
		char *dst, uint32_t maxdst);

/**
 * Dump arguments in a human readable form. Similar to pomp_decoder_dump() but
 * allocates the output buffer dynamically.
 * @param dec : decoder.
 * @param dst : destination buffer, which will be allocated to a suitable size
 * dynamically. Must be freed with free(), after usage.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_decoder_adump(struct pomp_decoder *dec, char **dst);

/**
 * Decode a 8-bit signed integer.
 * @param dec : decoder.
 * @param v : decoded value.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_decoder_read_i8(struct pomp_decoder *dec, int8_t *v);

/**
 * Decode a 8-bit unsigned integer.
 * @param dec : decoder.
 * @param v : decoded value.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_decoder_read_u8(struct pomp_decoder *dec, uint8_t *v);

/**
 * Decode a 16-bit signed integer.
 * @param dec : decoder.
 * @param v : decoded value.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_decoder_read_i16(struct pomp_decoder *dec, int16_t *v);

/**
 * Decode a 16-bit unsigned integer.
 * @param dec : decoder.
 * @param v : decoded value.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_decoder_read_u16(struct pomp_decoder *dec, uint16_t *v);

/**
 * Decode a 32-bit signed integer.
 * @param dec : decoder.
 * @param v : decoded value.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_decoder_read_i32(struct pomp_decoder *dec, int32_t *v);

/**
 * Decode a 32-bit unsigned integer.
 * @param dec : decoder.
 * @param v : decoded value.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_decoder_read_u32(struct pomp_decoder *dec, uint32_t *v);

/**
 * Decode a 64-bit signed integer.
 * @param dec : decoder.
 * @param v : decoded value.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_decoder_read_i64(struct pomp_decoder *dec, int64_t *v);

/**
 * Decode a 64-bit unsigned integer.
 * @param dec : decoder.
 * @param v : decoded value.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_decoder_read_u64(struct pomp_decoder *dec, uint64_t *v);

/**
 * Decode a string.
 * @param dec : decoder.
 * @param v : decoded string. Call 'free' when done.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_decoder_read_str(struct pomp_decoder *dec, char **v);

/**
 * Decode a string without any extra allocation or copy.
 * @param dec : decoder.
 * @param v : decoded string. Shall NOT be modified or freed as it points
 * directly to internal storage. Scope is the same as the associated message.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_decoder_read_cstr(struct pomp_decoder *dec, const char **v);

/**
 * Decode a buffer.
 * @param dec : decoder.
 * @param v : decoded buffer. Call 'free' when done.
 * @param n : decoded buffer size.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_decoder_read_buf(struct pomp_decoder *dec, void **v,
		uint32_t *n);

/**
 * Decode a buffer without any extra allocation or copy.
 * @param dec : decoder.
 * @param v : decoded buffer. Shall NOT be modified or freed as it points
 * directly to internal storage. Scope is the same as the associated message.
 * @param n : decoded buffer size.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_decoder_read_cbuf(struct pomp_decoder *dec, const void **v,
		uint32_t *n);

/**
 * Decode a 32-bit floating point.
 * @param dec : decoder.
 * @param v : decoded value.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_decoder_read_f32(struct pomp_decoder *dec, float *v);

/**
 * Decode a 64-bit floating point.
 * @param dec : decoder.
 * @param v : decoded value.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_API int pomp_decoder_read_f64(struct pomp_decoder *dec, double *v);

/**
 * Decode a file descriptor.
 * @param dec : decoder.
 * @param v : decoded value.
 * @return 0 in case of success, negative errno value in case of error.
 *
 * @remarks : the returned file descriptor shall NOT be closed. Duplicate it if
 * you need to use it after the decoder or the message is released.
 */
POMP_API int pomp_decoder_read_fd(struct pomp_decoder *dec, int *v);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_LIBPOMP_H_ */
