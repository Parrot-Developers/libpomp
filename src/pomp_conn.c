/**
 * @file pomp_conn.c
 *
 * @brief Handle read/write IO of a socket connection.
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
 *   * Neither the name of the <organization> nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "pomp_priv.h"

/** Internal read buffer size */
#define POMP_CONN_READ_SIZE	4096

/**
 * Determine if a read/write error in non-blocking could not be completed.
 * POSIX.1-2001 allows either error to be returned for this case, and
 * does not require these constants to have the same value, so a portable
 * application should check for both possibilities. */
#define POMP_CONN_WOULD_BLOCK(_err) \
	( \
		(_err) == EAGAIN || \
		(_err) == EWOULDBLOCK \
	)

/**
 * Determine if a connection is a local unix socket.
 */
#define POMP_CONN_IS_LOCAL(_conn) \
	((_conn)->local_addr.ss_family == AF_UNIX)

/** IO buffer for asynchronous write operations */
struct pomp_io_buffer {
	size_t			len;	/**< Buffer size */
	size_t			off;	/**< Offset in buffer */
	struct pomp_buffer	*buf;	/**< Associated buffer data */
	struct pomp_io_buffer	*next;	/**< Next IO buffer in chain */
};

/** Connection structure */
struct pomp_conn {
	/** Associated client/server context */
	struct pomp_ctx		*ctx;

	/** Associated loop */
	struct pomp_loop	*loop;

	/** Socket fd for read/write operations */
	int			fd;

	/** Flag indicating that connection shall be removed from context */
	int			removeflag;

	/** Read buffer */
	uint8_t			readbuf[POMP_CONN_READ_SIZE];

	/** To chain connection structures in server context */
	struct pomp_conn	*next;

	/** Protocol state */
	struct pomp_prot	*prot;

	/** Pending write head io buffer */
	struct pomp_io_buffer	*headbuf;

	/** Pending write tail io buffer */
	struct pomp_io_buffer	*tailbuf;

	/** Local address */
	struct sockaddr_storage	local_addr;

	/** Local address size */
	socklen_t		local_addrlen;

	/** Remote peer address */
	struct sockaddr_storage	peer_addr;

	/** Remote peer address size */
	socklen_t		peer_addrlen;

	/** Remote peer credential for local sockets */
	struct ucred		peer_cred;

	/** Received file descriptors on local unix socket connection */
	int			*fds;

	/** Number of file descriptors */
	size_t			fdcount;

	/** Maximum number of file descriptors in 'fds' array */
	size_t			fdmax;
};

/**
 * Create a new IO buffer.
 * @param buf : buffer with data to write.
 * @param off : offset in buffer of next byte to write.
 * @return IO buffer or NULL in case of error.
 *
 * @remarks a new reference on the buffer is taken, making it read-only.
 */
static struct pomp_io_buffer *pomp_io_buffer_new(struct pomp_buffer *buf,
		size_t off)
{
	struct pomp_io_buffer *iobuf = NULL;

	/* Allocate iobuf structure */
	iobuf = calloc(1, sizeof(*iobuf));
	if (iobuf == NULL)
		return NULL;

	/* Setup buffer */
	iobuf->len = buf->len;
	iobuf->off = off;
	iobuf->buf = buf;
	pomp_buffer_ref(buf);
	return iobuf;
}

/**
 * Destroy an IO buffer.
 * @param iobuf : IO buffer.
 * @return 0 in case of success, negative errno value in case of error.
 *
 * @remarks the associated buffer is just unreferenced.
 */
static int pomp_io_buffer_destroy(struct pomp_io_buffer *iobuf)
{
	pomp_buffer_unref(iobuf->buf);
	free(iobuf);
	return 0;
}

static int pomp_io_buffer_write_normal(struct pomp_io_buffer *iobuf,
		struct pomp_conn *conn)
{
	ssize_t writelen = 0;

	/* Write data ignoring interrupts */
	do {
		writelen = write(conn->fd, iobuf->buf->data + iobuf->off,
				iobuf->len - iobuf->off);
	} while (writelen < 0 && errno == EINTR);

	/* Log errors except EAGAIN */
	if (writelen < 0 && !POMP_CONN_WOULD_BLOCK(errno))
		POMP_LOG_FD_ERRNO("write", conn->fd);

	/* Return number of bytes written or error */
	return writelen < 0 ? -errno : (int)writelen;
}

/**
 * Write an IO buffer to the given connection with associated file descriptors
 * also transmitted as ancillary data. The internal offset is updated
 * in case of success.
 * @param iobuf : IO buffer.
 * @param conn : connection.
 * @return number of bytes written in case of success, negative errno value in
 * case of error. -EAGAIN is returned if write can not be completed immediately.
 */
static int pomp_io_buffer_write_with_fds(struct pomp_io_buffer *iobuf,
		struct pomp_conn *conn)
{
#ifdef SCM_RIGHTS
	ssize_t writelen = 0;
	struct iovec iov;
	struct msghdr msg;
	struct cmsghdr *cmsg = NULL;
	uint8_t cmsg_buf[CMSG_SPACE(POMP_BUFFER_MAX_FD_COUNT * sizeof(int))];
	uint32_t i = 0;
	int srcfd = 0;
	int *dstfd = 0;

	memset(&msg, 0, sizeof(msg));
	memset(&cmsg_buf, 0, sizeof(cmsg_buf));

	/* Setup the data part of the socket message */
	iov.iov_base = iobuf->buf->data + iobuf->off;
	iov.iov_len = iobuf->len - iobuf->off;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	/* Setup the control part of the socket message */
	msg.msg_control = cmsg_buf;
	msg.msg_controllen = CMSG_SPACE(iobuf->buf->fdcount * sizeof(int));
	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(iobuf->buf->fdcount * sizeof(int));

	/* Copy file descriptors */
	dstfd = (int *)CMSG_DATA(cmsg);
	for (i = 0; i < iobuf->buf->fdcount; i++) {
		srcfd = pomp_buffer_get_fd(iobuf->buf, iobuf->buf->fdoffs[i]);
		memcpy(&dstfd[i], &srcfd, sizeof(int));
	}

	/* Write data ignoring interrupts */
	do {
		writelen = sendmsg(conn->fd, &msg, 0);
	} while (writelen < 0 && errno == EINTR);

	/* Log errors except EAGAIN */
	if (writelen < 0 && !POMP_CONN_WOULD_BLOCK(errno))
		POMP_LOG_FD_ERRNO("sendmsg", conn->fd);

	/* Return number of bytes written or error */
	return writelen < 0 ? -errno : (int)writelen;
#else /* !SCM_RIGHTS */
	return pomp_io_buffer_write_normal(iobuf, conn);
#endif /* !SCM_RIGHTS */
}

/**
 * Write an IO buffer to the given connection. The internal offset is updated
 * in case of success.
 * @param iobuf : IO buffer.
 * @param conn : connection.
 * @return 0 in case of success, negative errno value in case of error.
 * -EAGAIN is returned if write can not be completed immediately.
 */
static int pomp_io_buffer_write(struct pomp_io_buffer *iobuf,
		struct pomp_conn *conn)
{
	int res = 0;

	/* When offset is 0 and buffer has file descriptors in it, write them */
	if (iobuf->off == 0 && iobuf->buf->fdcount > 0)
		res = pomp_io_buffer_write_with_fds(iobuf, conn);
	else
		res = pomp_io_buffer_write_normal(iobuf, conn);
	if (res < 0)
		return res;

	/* Update internal offset */
	iobuf->off += (size_t)res;
	return 0;
}

static int pomp_conn_add_rx_fd(struct pomp_conn *conn, int fd)
{
	int *newfds = NULL;
	size_t step = 0, i = 0;

	/* Make sure array is big enough */
	if (conn->fdcount >= conn->fdmax) {
		/* Add 4 more entries in array */
		step = 4;
		newfds = realloc(conn->fds, (conn->fdmax + step) * sizeof(int));
		if (newfds == NULL)
			return -ENOMEM;
		conn->fds = newfds;

		/* Initialize new entries at -1 */
		for (i = 0; i < step; i++)
			conn->fds[conn->fdmax + i] = -1;
		conn->fdmax += step;
	}

	/* Add at end of array */
	conn->fds[conn->fdcount] = fd;
	conn->fdcount++;
	return 0;
}

static int pomp_conn_get_next_rx_fd(struct pomp_conn *conn)
{
	int fd = 0;

	/* Do we have some entries in the array */
	if (conn->fdcount == 0) {
		POMP_LOGE("Not enough file descriptors received");
		return -1;
	}

	/* Get first one, move others */
	fd = conn->fds[0];
	conn->fdcount--;
	memmove(&conn->fds[0], &conn->fds[1], conn->fdcount * sizeof(int));
	conn->fds[conn->fdcount] = -1;
	return fd;
}

static int pomp_conn_clear_rx_fds(struct pomp_conn *conn)
{
	uint32_t i = 0;

	/* Close received file descriptors */
	if (conn->fds != NULL) {
		for (i = 0; i < conn->fdcount; i++) {
			if (close(conn->fds[i]) < 0)
				POMP_LOG_FD_ERRNO("close", conn->fds[i]);
		}
		free(conn->fds);
		conn->fds = NULL;
	}

	return 0;
}

static int pomp_conn_fixup_rx_fds_cb(struct pomp_decoder *dec, uint8_t type,
		const union pomp_value *v, uint32_t buflen, void *userdata)
{
	int res = 0;
	int fd = -1;
	struct pomp_conn *conn = userdata;
	size_t off = 0;

	/* Only interested in file descriptors */
	if (type != POMP_PROT_DATA_TYPE_FD)
		goto out;

	/* Get next file descriptor in rx array, always register offset as
	 * holding a file descriptor even if it is invalid */
	fd = pomp_conn_get_next_rx_fd(conn);
	off = dec->pos - sizeof(int32_t);
	res = pomp_buffer_register_fd(dec->msg->buf, off, fd);

	/* If fd was not put in buffer, we need to close it here */
	if (res < 0 && fd >= 0 && close(fd) < 0)
		POMP_LOG_FD_ERRNO("close", fd);

out:
	/* Continue if no errors */
	return res == 0;
}

static int pomp_conn_fixup_rx_fds(struct pomp_conn *conn,
		struct pomp_msg *msg)
{
	int res = 0;
	struct pomp_decoder dec = POMP_DECODER_INITIALIZER;

	/* Walk message t find file descriptors */
	(void)pomp_decoder_init(&dec, msg);
	res = pomp_decoder_walk(&dec, &pomp_conn_fixup_rx_fds_cb, conn, 0);

	/* At this point, if there is still some file descriptors in rx array
	 * it means we received too much. They can not belong to another
	 * message because recvmsg does NOT merge packets of different
	 * messages */
	if (conn->fdcount != 0) {
		/* Close extra file descriptors here but keep message */
		POMP_LOGE("Too many file descriptors received");
		pomp_conn_clear_rx_fds(conn);
	}

	/* Always clear decoder, even in case of error during decoding */
	(void)pomp_decoder_clear(&dec);
	return res;
}

/**
 * Function called when some data have been read on the connection fd. It
 * tries to decode a message and notify the associated context when a full
 * message has been successfully parsed.
 * @param conn : connection.
 * @param buf : data read.
 * @param len : size of data.
 */
static void pomp_conn_process_read_buf(struct pomp_conn *conn, const void *buf,
		size_t len)
{
	size_t off = 0;
	ssize_t usedlen = 0;
	struct pomp_msg *msg = NULL;

	/* Decoding loop */
	while (off < len) {
		usedlen = pomp_prot_decode_msg(conn->prot,
				((const uint8_t *)buf) + off,
				len - off, &msg);
		if (usedlen < 0)
			break;
		off += (size_t)usedlen;

		/* Notify new received message
		 * (only if file descriptor fixup is OK) */
		if (msg != NULL) {
			/* Always do the fixup even for inet sockets to at least
			 * put some invalid markers */
			if (pomp_conn_fixup_rx_fds(conn, msg) == 0)
				pomp_ctx_notify_msg(conn->ctx, conn, msg);
			pomp_prot_release_msg(conn->prot, msg);
			msg = NULL;
		}
	}
}

static int pomp_conn_process_read_normal(struct pomp_conn *conn)
{
	ssize_t readlen = 0;

	/* Read data ignoring interrupts */
	do {
		readlen = read(conn->fd, conn->readbuf,
				POMP_CONN_READ_SIZE);
	} while (readlen < 0 && errno == EINTR);

	/* Log errors except EAGAIN */
	if (readlen < 0 && !POMP_CONN_WOULD_BLOCK(errno))
		POMP_LOG_FD_ERRNO("read", conn->fd);

	/* Return number of bytes read or error */
	return readlen < 0 ? -errno : (int)readlen;
}

static int pomp_conn_process_read_with_fds(struct pomp_conn *conn)
{
#ifdef SCM_RIGHTS
	int res = 0;
	ssize_t readlen = 0;
	struct iovec iov;
	struct msghdr msg;
	struct cmsghdr *cmsg = NULL;
	uint8_t cmsg_buf[CMSG_SPACE(POMP_BUFFER_MAX_FD_COUNT * sizeof(int))];
	size_t i = 0, nfd = 0;
	int *srcfds = 0;

	memset(&msg, 0, sizeof(msg));

	/* Setup the data part of the socket message */
	iov.iov_base = conn->readbuf;
	iov.iov_len = POMP_CONN_READ_SIZE;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	/* Setup the control part of the socket message */
	msg.msg_control = cmsg_buf;
	msg.msg_controllen = sizeof(cmsg_buf);

	/* Read data ignoring interrupts */
	do {
		readlen = recvmsg(conn->fd, &msg, 0);
	} while (readlen < 0 && errno == EINTR);

	/* Log errors except EAGAIN */
	if (readlen < 0 && !POMP_CONN_WOULD_BLOCK(errno))
		POMP_LOG_FD_ERRNO("recvmsg", conn->fd);

	/* Return immediately in case of error or EOF */
	if (readlen < 0)
		return -errno;
	if (readlen == 0)
		return 0;

	/* Process ancillary data */
	for (cmsg = CMSG_FIRSTHDR(&msg);
			cmsg != NULL;
			cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_level != SOL_SOCKET)
			continue;
		if (cmsg->cmsg_type != SCM_RIGHTS)
			continue;

		/* Add received file descriptors in our array */
		nfd = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
		srcfds = (int *)CMSG_DATA(cmsg);
		for (i = 0; i < nfd; i++) {
			/* Only add if no previous errors */
			if (res == 0)
				res = pomp_conn_add_rx_fd(conn, srcfds[i]);

			/* Close if the fd was not registered */
			if (res < 0)
				close(srcfds[i]);
		}
	}

	/* Return number of bytes read or error */
	return res < 0 ? res : (int)readlen;
#else /* !SCM_RIGHTS */
	return pomp_conn_process_read_normal(conn);
#endif /* !SCM_RIGHTS */
}

/**
 * Function called when the fd is readable. It reads as many bytes as possible
 * until either there is no more data immediately available ('read' returned
 * EAGAIN) or EOF is reached or another error is returned by 'read'.
 * @param conn : connection.
 */
static void pomp_conn_process_read(struct pomp_conn *conn)
{
	int res = 0;

	do {
		/* Read data */
		if (POMP_CONN_IS_LOCAL(conn))
			res = pomp_conn_process_read_with_fds(conn);
		else
			res = pomp_conn_process_read_normal(conn);

		/* Process read data */
		if (res > 0) {
			pomp_conn_process_read_buf(conn, conn->readbuf,
					(size_t)res);
		} else if (res == 0 || !POMP_CONN_WOULD_BLOCK(-res)) {
			/* Error or EOF, finish this connection */
			conn->removeflag = 1;
		}
	} while (res > 0);
}

/**
 * Function called when the fd is writable and there is some IO buffer pending.
 * It resumes writing until either there is no more pending IO buffer or
 * data can not be written immediately ('write' returned EAGAIN).
 * @param conn : connection.
 */
static void pomp_conn_process_write(struct pomp_conn *conn)
{
	int res = 0;
	struct pomp_io_buffer *iobuf = NULL;

	/* Write pending buffers */
	iobuf = conn->headbuf;
	while (iobuf != NULL) {
		/* Try to write buffer */
		res = pomp_io_buffer_write(iobuf, conn);
		if (POMP_CONN_WOULD_BLOCK(-res)) {
			break;
		} else if (res < 0) {
			/* Error, finish this connection */
			conn->removeflag = 1;
			break;
		}

		/* Remove pending buffer if completed */
		if (iobuf->off == iobuf->len) {
			conn->headbuf = iobuf->next;
			if (conn->headbuf == NULL)
				conn->tailbuf = NULL;
			pomp_io_buffer_destroy(iobuf);
			iobuf = conn->headbuf;
		}
	}

	/* If queue is empty, stop monitoring OUT events */
	if (conn->headbuf == NULL) {
		POMP_LOGD("conn=%p fd=%d exit async mode", conn, conn->fd);
		pomp_loop_update(conn->loop, conn->fd, POMP_FD_EVENT_IN);
	}
}

/**
 * Function called when the fd of the connection has an event to process.
 * @param fd : fd of the connection .
 * @param revents : events to process.
 * @param userdata : connection object.
 */
static void pomp_conn_cb(int fd, uint32_t revents, void *userdata)
{
	struct pomp_conn *conn = userdata;
	if (!conn->removeflag && (revents & POMP_FD_EVENT_IN))
		pomp_conn_process_read(conn);
	if (!conn->removeflag && (revents & POMP_FD_EVENT_OUT))
		pomp_conn_process_write(conn);
	if (conn->removeflag
			|| (revents & POMP_FD_EVENT_HUP)
			|| (revents & POMP_FD_EVENT_ERR)) {
		pomp_ctx_remove_conn(conn->ctx, conn);
	}
}

/**
 * Create a new connection object to wrap read/write operations on a fd.
 * @param ctx : associated context.
 * @param loop : associated loop.
 * @param fd : fd to wrap.
 * @return connection object or NULL in case of error.
 */
struct pomp_conn *pomp_conn_new(struct pomp_ctx *ctx,
		struct pomp_loop *loop, int fd)
{
	int res = 0;
	struct pomp_conn *conn = NULL;
#ifdef SO_PEERCRED
	socklen_t optlen = 0;
#endif /* SO_PEERCRED */

	POMP_RETURN_VAL_IF_FAILED(ctx != NULL, -EINVAL, NULL);
	POMP_RETURN_VAL_IF_FAILED(loop != NULL, -EINVAL, NULL);
	POMP_RETURN_VAL_IF_FAILED(fd >= 0, -EINVAL, NULL);

	/* Allocate conn structure */
	conn = calloc(1, sizeof(*conn));
	if (conn == NULL)
		goto error;

	/* Initialize structure */
	conn->ctx = ctx;
	conn->loop = loop;
	conn->fd = fd;
	conn->removeflag = 0;
	conn->prot = pomp_prot_new();
	if (conn->prot == NULL)
		goto error;

	/* Always monitor IN events */
	res = pomp_loop_add(conn->loop, conn->fd, POMP_FD_EVENT_IN,
			&pomp_conn_cb, conn);
	if (res < 0)
		goto error;

	/* Get local address information */
	conn->local_addrlen = sizeof(conn->local_addr);
	if (getsockname(fd, (struct sockaddr *)&conn->local_addr,
			&conn->local_addrlen) < 0) {
		POMP_LOG_FD_ERRNO("getsockname", fd);
		conn->local_addrlen = 0;
	}

	/* Get peer address information */
	conn->peer_addrlen = sizeof(conn->peer_addr);
	if (getpeername(fd, (struct sockaddr *)&conn->peer_addr,
			&conn->peer_addrlen) < 0) {
		res = -errno;
		POMP_LOG_FD_ERRNO("getpeername", fd);
		conn->peer_addrlen = 0;

		/* Do NOT ignore the 'peer not connected' error, abort now */
		if (res == -ENOTCONN)
			goto error;
	}

	/* Get peer credentials information */
#ifdef SO_PEERCRED
	optlen = sizeof(conn->peer_cred);
	if (conn->peer_addr.ss_family == AF_UNIX) {
		if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED,
				&conn->peer_cred, &optlen) < 0) {
			POMP_LOG_FD_ERRNO("getsockopt.SO_PEERCRED", fd);
		}
	}
#endif /* SO_PEERCRED */

	return conn;

	/* Cleanup in case of error */
error:
	if (conn != NULL) {
		if (conn->prot != NULL)
			pomp_prot_destroy(conn->prot);
		pomp_loop_remove(conn->loop, conn->fd);
		free(conn);
	}
	return NULL;
}

/**
 * Destroy a connection object.
 * @param conn : connection.
 * @return 0 in case of success, negative errno value in case of error.
 * If the fd is still opened, -EBUSY is returned.
 */
int pomp_conn_destroy(struct pomp_conn *conn)
{
	POMP_RETURN_ERR_IF_FAILED(conn != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(conn->fd < 0, -EBUSY);
	pomp_prot_destroy(conn->prot);
	free(conn);
	return 0;
}

/**
 * Close the connection. It properly shutdowns the socket connection and close
 * the fd.
 * @param conn : connection.
 * @return 0 in case of success, negative errno value in case of error.
 */
int pomp_conn_close(struct pomp_conn *conn)
{
	POMP_RETURN_ERR_IF_FAILED(conn != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(conn->fd >= 0, -EINVAL);

	/* Close remaining received file descriptors */
	pomp_conn_clear_rx_fds(conn);

	/* Properly shutdown the connection */
	if (shutdown(conn->fd, SHUT_RDWR) < 0)
		POMP_LOG_FD_ERRNO("shutdown", conn->fd);
	pomp_loop_remove(conn->loop, conn->fd);

	/* Release resources */
	close(conn->fd);
	conn->fd = -1;
	return 0;
}

/**
 * Get the next connection.
 * @param conn : connection.
 * @return next connection or NULL in case of error or no next connection.
 */
struct pomp_conn *pomp_conn_get_next(const struct pomp_conn *conn)
{
	POMP_RETURN_VAL_IF_FAILED(conn != NULL, -EINVAL, NULL);
	return conn->next;
}

/**
 * Set the next connection.
 * @param conn : connection.
 * @param next : next connection.
 * @return 0 in case of success, negative errno value in case of error.
 */
int pomp_conn_set_next(struct pomp_conn *conn, struct pomp_conn *next)
{
	POMP_RETURN_ERR_IF_FAILED(conn != NULL, -EINVAL);
	conn->next = next;
	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_conn_disconnect(struct pomp_conn *conn)
{
	POMP_RETURN_ERR_IF_FAILED(conn != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(conn->fd >= 0, -ENOTCONN);

	/* Shutting down connection will ultimately cause an EOF */
	if (shutdown(conn->fd, SHUT_RDWR) < 0)
		POMP_LOG_FD_ERRNO("shutdown", conn->fd);
	return 0;
}

/*
 * See documentation in public header.
 */
const struct sockaddr *pomp_conn_get_local_addr(struct pomp_conn *conn,
		uint32_t *addrlen)
{
	POMP_RETURN_VAL_IF_FAILED(conn != NULL, -EINVAL, NULL);
	POMP_RETURN_VAL_IF_FAILED(addrlen != NULL, -EINVAL, NULL);
	*addrlen = conn->local_addrlen;
	return (const struct sockaddr *)&conn->local_addr;
}

/*
 * See documentation in public header.
 */
const struct sockaddr *pomp_conn_get_peer_addr(struct pomp_conn *conn,
		uint32_t *addrlen)
{
	POMP_RETURN_VAL_IF_FAILED(conn != NULL, -EINVAL, NULL);
	POMP_RETURN_VAL_IF_FAILED(addrlen != NULL, -EINVAL, NULL);
	*addrlen = conn->peer_addrlen;
	return (const struct sockaddr *)&conn->peer_addr;
}

/*
 * See documentation in public header.
 */
const struct ucred *pomp_conn_get_peer_cred(struct pomp_conn *conn)
{
	POMP_RETURN_VAL_IF_FAILED(conn != NULL, -EINVAL, NULL);
	if (conn->peer_addr.ss_family == AF_UNIX)
		return &conn->peer_cred;
	else
		return NULL;
}

/*
 * See documentation in public header.
 */
int pomp_conn_send_msg(struct pomp_conn *conn, const struct pomp_msg *msg)
{
	int res = 0;
	size_t off = 0;
	struct pomp_io_buffer *iobuf = NULL;
	struct pomp_io_buffer tmpiobuf;

	POMP_RETURN_ERR_IF_FAILED(conn != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(conn->fd >= 0, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(msg->buf != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(msg->buf->data != NULL, -EINVAL);

	/* If buffer has file descriptors in it, the connection must be a
	 * local unix socket */
	if (msg->buf->fdcount > 0 && !POMP_CONN_IS_LOCAL(conn)) {
		POMP_LOGE("Unable to send message with file descriptors");
		return -EPERM;
	}

	/* Try to send now if possible */
	if (conn->headbuf == NULL) {
		/* Prepare a local temp io buffer */
		memset(&tmpiobuf, 0, sizeof(tmpiobuf));
		tmpiobuf.buf = msg->buf;
		tmpiobuf.len = msg->buf->len;
		tmpiobuf.off = 0;
		tmpiobuf.next = NULL;

		/* Write it */
		res = pomp_io_buffer_write(&tmpiobuf, conn);
		if (res < 0) {
			if (!POMP_CONN_WOULD_BLOCK(-res))
				return res;
		} else if (tmpiobuf.off == tmpiobuf.len) {
			/* If everything was written, nothing more to do */
			return 0;
		} else {
			off = tmpiobuf.off;
		}
	}

	/* Need to queue the buffer */
	iobuf = pomp_io_buffer_new(msg->buf, off);
	if (iobuf == NULL)
		return -ENOMEM;

	POMP_LOGD("conn=%p fd=%d enter async mode", conn, conn->fd);
	if (conn->tailbuf == NULL) {
		/* No previous pending buffer */
		conn->headbuf = iobuf;
		conn->tailbuf = iobuf;
		pomp_loop_update(conn->loop, conn->fd,
				POMP_FD_EVENT_IN | POMP_FD_EVENT_OUT);
	} else {
		/* Simply add tail */
		conn->tailbuf->next = iobuf;
		conn->tailbuf = iobuf;
	}

	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_conn_send(struct pomp_conn *conn, uint32_t msgid, const char *fmt, ...)
{
	int res = 0;
	va_list args;
	va_start(args, fmt);
	res = pomp_conn_sendv(conn, msgid, fmt, args);
	va_end(args);
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_conn_sendv(struct pomp_conn *conn, uint32_t msgid, const char *fmt,
		va_list args)
{
	int res = 0;
	struct pomp_msg msg = POMP_MSG_INITIALIZER;

	/* Write message and send it*/
	res = pomp_msg_writev(&msg, msgid, fmt, args);
	if (res == 0)
		res = pomp_conn_send_msg(conn, &msg);

	/* Always cleanup message */
	(void)pomp_msg_clear(&msg);
	return res;
}
