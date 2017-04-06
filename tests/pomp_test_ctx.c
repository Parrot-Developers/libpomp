/**
 * @file pomp_test_ctx.c
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

#include "pomp_test.h"

struct test_peer {
	struct pomp_ctx		*ctx;
	const struct sockaddr	*addr;
	uint32_t		addrlen;
	uint8_t			recurs_send_enabled;
};

/** */
struct test_data {
	uint32_t  connection;
	uint32_t  disconnection;
	uint32_t  msg;
	uint32_t  buf;
	uint32_t  dataread;
	uint32_t  datasent;
	uint32_t  sendcount;
	uint8_t   isdisconnecting;
	int       isdgram;
	int       israw;
	struct test_peer srv;
	struct test_peer cli;
};

/** */
static void test_event_cb_t(struct pomp_ctx *ctx, enum pomp_event event,
		struct pomp_conn *conn, const struct pomp_msg *msg,
		void *userdata)
{
	int fd;
	int res = 0;
	struct test_data *data = userdata;
	const char *eventstr = pomp_event_str(event);
	const struct sockaddr *addr = NULL;
	uint32_t addrlen = 0;
	const struct pomp_cred *cred = NULL;
	int isunix = 0;

	switch (event) {
	case POMP_EVENT_CONNECTED:
		data->connection++;

		/* Invalid get fd (NULL param) */
		fd = pomp_conn_get_fd(NULL);
		CU_ASSERT_EQUAL(fd, -EINVAL);

		fd = pomp_conn_get_fd(conn);
		CU_ASSERT_TRUE(fd >= 0);

		addr = pomp_conn_get_local_addr(conn, &addrlen);
		CU_ASSERT_TRUE(addr != NULL);
		addr = pomp_conn_get_peer_addr(conn, &addrlen);
		CU_ASSERT_TRUE(addr != NULL);
		isunix = addr->sa_family == AF_UNIX;

		/* Invalid get addr (NULL param) */
		addr = pomp_conn_get_local_addr(NULL, &addrlen);
		CU_ASSERT_TRUE(addr == NULL);
		addr = pomp_conn_get_local_addr(conn, NULL);
		CU_ASSERT_TRUE(addr == NULL);
		addr = pomp_conn_get_peer_addr(NULL, &addrlen);
		CU_ASSERT_TRUE(addr == NULL);
		addr = pomp_conn_get_peer_addr(conn, NULL);
		CU_ASSERT_TRUE(addr == NULL);

		if (!isunix) {
			/* Invalid get credentials (bad type or NULL param) */
			cred = pomp_conn_get_peer_cred(conn);
			CU_ASSERT_TRUE(cred == NULL);
			cred = pomp_conn_get_peer_cred(NULL);
			CU_ASSERT_TRUE(cred == NULL);
		} else {
			cred = pomp_conn_get_peer_cred(conn);
			CU_ASSERT_TRUE(cred != NULL);
		}
		break;

	case POMP_EVENT_DISCONNECTED:
		data->disconnection++;
		break;

	case POMP_EVENT_MSG:
		data->msg++;
		if (pomp_msg_get_id(msg) == 1) {
			res = pomp_conn_send(conn, 2, "%s", eventstr);
			CU_ASSERT_EQUAL(res, 0);
		}

		if (data->isdgram) {
			res = pomp_conn_disconnect(conn);
			CU_ASSERT_EQUAL(res, -ENOTCONN);

			/* Internal function invalid arguments checks */
			res = pomp_conn_send_msg_to(NULL, msg, NULL, 0);
			CU_ASSERT_EQUAL(res, -EINVAL);
			res = pomp_conn_send_msg_to(conn, NULL, NULL, 0);
			CU_ASSERT_EQUAL(res, -EINVAL);
		} else {
			/* Internal function invalid arguments checks */
			res = pomp_conn_send_msg(NULL, msg);
			CU_ASSERT_EQUAL(res, -EINVAL);
			res = pomp_conn_send_msg(conn, NULL);
			CU_ASSERT_EQUAL(res, -EINVAL);
		}

		/* Internal function invalid arguments checks */
		res = pomp_ctx_notify_event(NULL, POMP_EVENT_CONNECTED, conn);
		CU_ASSERT_EQUAL(res, -EINVAL);
		res = pomp_ctx_notify_event(ctx, POMP_EVENT_MSG, conn);
		CU_ASSERT_EQUAL(res, -EINVAL);
		res = pomp_ctx_notify_event(ctx, POMP_EVENT_CONNECTED, NULL);
		CU_ASSERT_EQUAL(res, -EINVAL);
		res = pomp_ctx_notify_msg(NULL, conn, msg);
		CU_ASSERT_EQUAL(res, -EINVAL);
		res = pomp_ctx_notify_msg(ctx, NULL, msg);
		CU_ASSERT_EQUAL(res, -EINVAL);
		res = pomp_ctx_notify_msg(ctx, conn, NULL);
		CU_ASSERT_EQUAL(res, -EINVAL);
		break;

	default:
		CU_ASSERT_TRUE_FATAL(0);
		break;
	}
}

/** */
static void test_ctx_raw_cb(struct pomp_ctx *ctx,
		struct pomp_conn *conn,
		struct pomp_buffer *buf,
		void *userdata)
{
	int res = 0;
	size_t dataread = 0;
	struct test_data *data = userdata;
	data->buf++;

	/* count data received */
	res = pomp_buffer_get_cdata(buf, NULL, &dataread, NULL);
	CU_ASSERT_EQUAL(res, 0);
	data->dataread += dataread;

	if (data->isdgram) {
		res = pomp_conn_disconnect(conn);
		CU_ASSERT_EQUAL(res, -ENOTCONN);

		/* Internal function invalid arguments checks */
		res = pomp_conn_send_raw_buf_to(NULL, buf, NULL, 0);
		CU_ASSERT_EQUAL(res, -EINVAL);
		res = pomp_conn_send_raw_buf_to(conn, NULL, NULL, 0);
		CU_ASSERT_EQUAL(res, -EINVAL);
	}

	/* Internal function invalid arguments checks */
	res = pomp_ctx_notify_raw_buf(NULL, conn, buf);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_ctx_notify_raw_buf(ctx, NULL, buf);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_ctx_notify_raw_buf(ctx, conn, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);
}

/** */
static void test_ctx_socket_cb(struct pomp_ctx *ctx,
		int fd,
		enum pomp_socket_kind kind,
		void *userdata)
{
	const char *str = NULL;

	str = pomp_socket_kind_str(kind);
	CU_ASSERT_PTR_NOT_NULL(str);
}

/** */
static int send_msg(struct test_data *tdata, struct test_peer *src,
		struct test_peer *dst, char *msg)
{
	int res = 0;
	struct pomp_msg *pmsg = NULL;
	struct pomp_buffer *buf = NULL;

	/* Exchange some message */
	if (!tdata->isdgram) {
		if (!tdata->israw) {
			res = pomp_ctx_send(src->ctx, 3, "%s", msg);
		} else {
			buf = pomp_buffer_new(32);
			CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
			memcpy(buf->data, msg, strlen(msg));
			buf->len = strlen(msg);

			res = pomp_ctx_send_raw_buf(src->ctx, buf);
			if (tdata->isdisconnecting && src == &tdata->cli) {
				/* Check failed if send after client disconnect */
				CU_ASSERT_EQUAL(res, -ENOTCONN);
			} else {
				tdata->datasent += strlen(msg);
				CU_ASSERT_EQUAL(res, 0);
			}

			pomp_buffer_unref(buf);
			buf = NULL;
		}
	} else {
		if (!tdata->israw) {
			pmsg = pomp_msg_new();
			res = pomp_msg_clear(pmsg);
			CU_ASSERT_EQUAL(res, 0);
			res = pomp_msg_write(pmsg, 3, "%s", msg);
			CU_ASSERT_EQUAL(res, 0);
			res = pomp_ctx_send_msg_to(src->ctx, pmsg, dst->addr,
					dst->addrlen);
			if (tdata->isdisconnecting && src == &tdata->cli) {
				/* Check failed if send after client disconnect */
				CU_ASSERT_EQUAL(res, -ENOTCONN);
			} else {
				CU_ASSERT_EQUAL(res, 0);
			}

			res = pomp_msg_destroy(pmsg);
			CU_ASSERT_EQUAL(res, 0);
		} else {
			buf = pomp_buffer_new(32);
			CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
			memcpy(buf->data, msg, strlen(msg));
			buf->len = strlen(msg);

			res = pomp_ctx_send_raw_buf_to(src->ctx, buf, dst->addr,
					dst->addrlen);
			if (tdata->isdisconnecting && src == &tdata->cli) {
				/* Check failed if send after client disconnect */
				CU_ASSERT_EQUAL(res, -ENOTCONN);
			} else {
				tdata->datasent += strlen(msg);
				CU_ASSERT_EQUAL(res, 0);
			}

			pomp_buffer_unref(buf);
			buf = NULL;
		}
	}

	return res;
}

/** */
static void test_ctx_send_cb(struct pomp_ctx *ctx,
		struct pomp_conn *conn,
		struct pomp_buffer *buf,
		uint32_t status,
		void *cookie,
		void *userdata)
{
	int res = 0;
	struct test_data *data = userdata;
	struct test_peer *src = NULL;
	struct test_peer *dst = NULL;
	CU_ASSERT_PTR_NOT_NULL(ctx);
	CU_ASSERT_PTR_NOT_NULL(conn);
	CU_ASSERT_PTR_NOT_NULL(buf);
	data->sendcount++;
	CU_ASSERT_PTR_NULL(cookie);

	/* Internal function invalid arguments checks */
	res = pomp_ctx_notify_send(NULL, conn, buf, 0);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_ctx_notify_send(ctx, NULL, buf, 0);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_ctx_notify_send(ctx, conn, NULL, 0);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Get source and destination */
	src = (ctx == data->srv.ctx) ? &data->srv : &data->cli;
	dst = (ctx == data->srv.ctx) ? &data->cli : &data->srv;

	if (src->recurs_send_enabled) {
		/* Disable recursive send. */
		src->recurs_send_enabled = 0;

		/* Exchange some message */
		send_msg(data, src, dst, "recursive_msg");
	}
}

#ifdef __linux__

/** */
static void run_ctx(struct pomp_ctx *ctx1, struct pomp_ctx *ctx2, int timeout)
{
	int res = 0, nevts = 0;
	int fd1 = -1, fd2 = -1;
	struct pollfd pfd[2];

	/* Get fd of contexts */
	fd1 = pomp_ctx_get_fd(ctx1);
	CU_ASSERT_TRUE(fd1 >= 0);
	fd2 = pomp_ctx_get_fd(ctx2);
	CU_ASSERT_TRUE(fd2 >= 0);

	/* Run contexts */
	pfd[0].fd = fd1;
	pfd[0].events = POLLIN;
	pfd[1].fd = fd2;
	pfd[1].events = POLLIN;
	do {
		pfd[0].revents = 0;
		pfd[1].revents = 0;
		do {
			res = poll(pfd, 2, (int)timeout);
		} while (res < 0 && errno == EINTR);
		nevts = res;

		if (pfd[0].revents) {
			res = pomp_ctx_process_fd(ctx1);
			CU_ASSERT_EQUAL(res, 0);
		}

		if (pfd[1].revents) {
			res = pomp_ctx_process_fd(ctx2);
			CU_ASSERT_EQUAL(res, 0);
		}
	} while (nevts > 0);
	CU_ASSERT_EQUAL(res, 0);
}

#endif /* __linux__ */

#if defined(__FreeBSD__) || defined(__APPLE__)

struct run_ctx_data {
	struct pomp_ctx	*ctx;
	uint32_t	timeout;
};

static void *run_ctx_thread(void *arg)
{
	int res = 0;
	struct run_ctx_data *data = arg;
	do {
		res = pomp_ctx_wait_and_process(data->ctx, data->timeout);
	} while (res == 0);
	return 0;
}

static void run_ctx(struct pomp_ctx *ctx1, struct pomp_ctx *ctx2, int timeout)
{
	pthread_t thread1, thread2;
	struct run_ctx_data data1, data2;

	data1.ctx = ctx1;
	data1.timeout = timeout;
	pthread_create(&thread1, NULL, &run_ctx_thread,  &data1);

	data2.ctx = ctx2;
	data2.timeout = timeout;
	pthread_create(&thread2, NULL, &run_ctx_thread,  &data2);

	pthread_join(thread1, NULL);
	pthread_join(thread2, NULL);
}

#endif /* __FreeBSD__ || __APPLE__ */

#ifdef _WIN32

struct run_ctx_data {
	struct pomp_ctx	*ctx;
	uint32_t	timeout;
};

static DWORD WINAPI run_ctx_thread(void *arg)
{
	int res = 0;
	struct run_ctx_data *data = arg;
	do {
		res = pomp_ctx_wait_and_process(data->ctx, data->timeout);
	} while (res == 0);
	return 0;
}

static void run_ctx(struct pomp_ctx *ctx1, struct pomp_ctx *ctx2, int timeout)
{
	HANDLE hthread1 = NULL, hthread2 = NULL;
	DWORD threadid1 = 0, threadid2 = 0;
	struct run_ctx_data data1, data2;

	data1.ctx = ctx1;
	data1.timeout = timeout;
	hthread1 = CreateThread(NULL, 0, &run_ctx_thread, &data1, 0, &threadid1);

	data2.ctx = ctx2;
	data2.timeout = timeout;
	hthread2 = CreateThread(NULL, 0, &run_ctx_thread, &data2, 0, &threadid2);

	WaitForSingleObject(hthread1, INFINITE);
	WaitForSingleObject(hthread2, INFINITE);
	CloseHandle(hthread1);
	CloseHandle(hthread2);
}

#endif /* _WIN32 */

/** */
static void test_ctx(const struct sockaddr *addr1, uint32_t addrlen1,
		const struct sockaddr *addr2, uint32_t addrlen2,
		int isdgram, int israw, int withsockcb, int withsendcb)
{
	int res = 0;
	struct test_data data;
	struct pomp_loop *loop = NULL;
	struct pomp_conn *conn = NULL;
	struct pomp_msg *msg = NULL;
	int fd = -1;
	uint32_t i = 0, j = 0;
	struct pomp_buffer *buf = NULL;

	memset(&data, 0, sizeof(data));
	data.isdgram = isdgram;
	data.israw = israw;
	data.srv.addr = addr1;
	data.srv.addrlen = addrlen1;
	data.cli.addr = addr2;
	data.cli.addrlen = addrlen2;
	msg = pomp_msg_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(msg);

	/* Create context */
	data.srv.ctx = pomp_ctx_new(&test_event_cb_t, &data);
	CU_ASSERT_PTR_NOT_NULL_FATAL(data.srv.ctx);
	if (israw) {
		res = pomp_ctx_set_raw(data.srv.ctx, &test_ctx_raw_cb);
		CU_ASSERT_EQUAL(res, 0);
	}
	if (withsockcb) {
		res = pomp_ctx_set_socket_cb(data.srv.ctx, &test_ctx_socket_cb);
		CU_ASSERT_EQUAL(res, 0);
	}
	if (withsendcb) {
		res = pomp_ctx_set_send_cb(data.srv.ctx, &test_ctx_send_cb);
		CU_ASSERT_EQUAL(res, 0);
	}

	/* Create context without callback */
	data.cli.ctx = pomp_ctx_new(NULL, &data);
	CU_ASSERT_PTR_NOT_NULL_FATAL(data.cli.ctx);
	res = pomp_ctx_destroy(data.cli.ctx);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid create (NULL 3nd arg) */
	data.cli.ctx = pomp_ctx_new_with_loop(NULL, &data, NULL);
	CU_ASSERT_PTR_NULL(data.cli.ctx);
	data.cli.ctx = pomp_ctx_new_with_loop(&test_event_cb_t, &data, NULL);
	CU_ASSERT_PTR_NULL(data.cli.ctx);

	/* Create 2nd context */
	data.cli.ctx = pomp_ctx_new(&test_event_cb_t, &data);
	CU_ASSERT_PTR_NOT_NULL_FATAL(data.cli.ctx);
	if (israw) {
		res = pomp_ctx_set_raw(data.cli.ctx, &test_ctx_raw_cb);
		CU_ASSERT_EQUAL(res, 0);
	}
	if (withsockcb) {
		res = pomp_ctx_set_socket_cb(data.cli.ctx, &test_ctx_socket_cb);
		CU_ASSERT_EQUAL(res, 0);
	}
	if (withsendcb) {
		res = pomp_ctx_set_send_cb(data.cli.ctx, &test_ctx_send_cb);
		CU_ASSERT_EQUAL(res, 0);
	}

	if (!isdgram) {
		/* Invalid start server (NULL param) */
		res = pomp_ctx_listen(NULL, addr1, addrlen1);
		CU_ASSERT_EQUAL(res, -EINVAL);
		res = pomp_ctx_listen(data.srv.ctx, NULL, addrlen1);
		CU_ASSERT_EQUAL(res, -EINVAL);

		/* Start as server 1st context */
		res = pomp_ctx_listen(data.srv.ctx, addr1, addrlen1);
		CU_ASSERT_EQUAL(res, 0);

		/* Invalid start server (busy) */
		res = pomp_ctx_listen(data.srv.ctx, addr1, addrlen1);
		CU_ASSERT_EQUAL(res, -EBUSY);
	} else {
		/* Invalid bind (NULL param) */
		res = pomp_ctx_bind(NULL, addr1, addrlen1);
		CU_ASSERT_EQUAL(res, -EINVAL);
		res = pomp_ctx_bind(data.srv.ctx, NULL, addrlen1);
		CU_ASSERT_EQUAL(res, -EINVAL);

		/* Bind 1st context */
		res = pomp_ctx_bind(data.srv.ctx, addr1, addrlen1);
		CU_ASSERT_EQUAL(res, 0);

		/* Invalid bind (busy) */
		res = pomp_ctx_bind(data.srv.ctx, addr1, addrlen1);
		CU_ASSERT_EQUAL(res, -EBUSY);
	}

	if (!isdgram) {
		/* Invalid start client (NULL param) */
		res = pomp_ctx_connect(NULL, addr2, addrlen2);
		CU_ASSERT_EQUAL(res, -EINVAL);
		res = pomp_ctx_connect(data.cli.ctx, NULL, addrlen2);
		CU_ASSERT_EQUAL(res, -EINVAL);

		/* Start as client 2nd context */
		res = pomp_ctx_connect(data.cli.ctx, addr2, addrlen2);
		CU_ASSERT_EQUAL(res, 0);

		/* Invalid start client (busy) */
		res = pomp_ctx_connect(data.cli.ctx, addr2, addrlen2);
		CU_ASSERT_EQUAL(res, -EBUSY);
	} else {
		/* Invalid bind (NULL param) */
		res = pomp_ctx_bind(NULL, addr2, addrlen2);
		CU_ASSERT_EQUAL(res, -EINVAL);
		res = pomp_ctx_bind(data.cli.ctx, NULL, addrlen2);
		CU_ASSERT_EQUAL(res, -EINVAL);

		/* Bind 2nd context */
		res = pomp_ctx_bind(data.cli.ctx, addr2, addrlen2);
		CU_ASSERT_EQUAL(res, 0);

		/* Invalid bind (busy) */
		res = pomp_ctx_bind(data.cli.ctx, addr2, addrlen2);
		CU_ASSERT_EQUAL(res, -EBUSY);
	}

	/* Invalid set raw */
	res = pomp_ctx_set_raw(NULL, &test_ctx_raw_cb);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_ctx_set_raw(data.srv.ctx, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_ctx_set_raw(data.srv.ctx, &test_ctx_raw_cb);
	CU_ASSERT_EQUAL(res, -EBUSY);

	/* Invalid set socket cb */
	res = pomp_ctx_set_socket_cb(NULL, &test_ctx_socket_cb);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_ctx_set_socket_cb(data.srv.ctx, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_ctx_set_socket_cb(data.srv.ctx, &test_ctx_socket_cb);
	CU_ASSERT_EQUAL(res, -EBUSY);

	/* Invalid set send cb */
	res = pomp_ctx_set_send_cb(NULL, &test_ctx_send_cb);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_ctx_set_send_cb(data.srv.ctx, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_ctx_set_send_cb(data.srv.ctx, &test_ctx_send_cb);
	CU_ASSERT_EQUAL(res, -EBUSY);

	/* Invalid get loop (NULL param) */
	loop = pomp_ctx_get_loop(NULL);
	CU_ASSERT_PTR_NULL(loop);

	/* Invalid get fd (NULL param) */
	fd = pomp_ctx_get_fd(NULL);
	CU_ASSERT_EQUAL(fd, -EINVAL);

	/* Get loop and fd */
	loop = pomp_ctx_get_loop(data.srv.ctx);
	CU_ASSERT_PTR_NOT_NULL(loop);
	fd = pomp_ctx_get_fd(data.srv.ctx);
#ifdef POMP_HAVE_LOOP_EPOLL
	CU_ASSERT_TRUE(fd >= 0);
#else
	CU_ASSERT_EQUAL(fd, -ENOSYS);
#endif
	/* Invalid process fd (NULL param) */
	res = pomp_ctx_process_fd(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Keepalive settings */
	if (!isdgram) {
		/* TODO: check that it actually does something */
		res = pomp_ctx_setup_keepalive(data.srv.ctx, 0, 0, 0, 0);
		CU_ASSERT_EQUAL(res, 0);
		res = pomp_ctx_setup_keepalive(data.srv.ctx, 1, 5, 2, 1);
		CU_ASSERT_EQUAL(res, 0);
		res = pomp_ctx_setup_keepalive(NULL, 0, 0, 0, 0);
		CU_ASSERT_EQUAL(res, -EINVAL);
	}

	/* Run contexts (they shall connect each other) */
	run_ctx(data.srv.ctx, data.cli.ctx, 100);
	if (!isdgram) {
		CU_ASSERT_EQUAL(data.connection, 2);

		/* Get remote connections */
		conn = pomp_ctx_get_next_conn(data.srv.ctx, NULL);
		CU_ASSERT_PTR_NOT_NULL(conn);
		conn = pomp_ctx_get_next_conn(data.srv.ctx, conn);
		CU_ASSERT_PTR_NULL(conn);
		conn = pomp_ctx_get_conn(data.cli.ctx);
		CU_ASSERT_PTR_NOT_NULL(conn);

		/* Invalid get remote connections */
		conn = pomp_ctx_get_next_conn(data.cli.ctx, NULL);
		CU_ASSERT_PTR_NULL(conn);
		conn = pomp_ctx_get_conn(data.srv.ctx);
		CU_ASSERT_PTR_NULL(conn);
		conn = pomp_ctx_get_next_conn(NULL, NULL);
		CU_ASSERT_PTR_NULL(conn);
		conn = pomp_ctx_get_conn(NULL);
		CU_ASSERT_PTR_NULL(conn);
	}

	/* Exchange some message */
	if (!isdgram) {
		if (!israw) {
			res = pomp_ctx_send(data.srv.ctx, 1, "%s", "hello1->2");
			CU_ASSERT_EQUAL(res, 0);
			res = pomp_ctx_send(data.cli.ctx, 1, "%s", "hello2->1");
			CU_ASSERT_EQUAL(res, 0);

			/* Invalid send (NULL param) */
			res = pomp_ctx_send(NULL, 1, "%s", "hello1->2");
			CU_ASSERT_EQUAL(res, -EINVAL);
			res = pomp_ctx_send_msg(data.srv.ctx, NULL);
			CU_ASSERT_EQUAL(res, -EINVAL);

			/* Invalid send (bad format) */
			res = pomp_ctx_send(data.srv.ctx, 1, "%o", 1);
			CU_ASSERT_EQUAL(res, -EINVAL);
			res = pomp_conn_send(pomp_ctx_get_conn(data.cli.ctx), 1, "%o", 1);
			CU_ASSERT_EQUAL(res, -EINVAL);

			/* Invalid send to (bad type) */
			res = pomp_ctx_send_msg_to(data.cli.ctx, msg, addr1, addrlen1);
			CU_ASSERT_EQUAL(res, -EINVAL);
		} else {
			buf = pomp_buffer_new(32);
			CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
			memcpy(buf->data, "Hello World !!!", 15);
			buf->len = 15;

			res = pomp_ctx_send_raw_buf(data.srv.ctx, buf);
			data.datasent += 15;
			CU_ASSERT_EQUAL(res, 0);
			res = pomp_ctx_send_raw_buf(data.cli.ctx, buf);
			data.datasent += 15;
			CU_ASSERT_EQUAL(res, 0);

			/* Invalid send (NULL param) */
			res = pomp_ctx_send_raw_buf(NULL, buf);
			CU_ASSERT_EQUAL(res, -EINVAL);
			res = pomp_ctx_send_raw_buf(data.srv.ctx, NULL);
			CU_ASSERT_EQUAL(res, -EINVAL);

			pomp_buffer_unref(buf);
			buf = NULL;
		}
	} else {
		if (!israw) {
			res = pomp_msg_clear(msg);
			CU_ASSERT_EQUAL(res, 0);
			res = pomp_msg_write(msg, 1, "%s", "hello1->2");
			CU_ASSERT_EQUAL(res, 0);
			res = pomp_ctx_send_msg_to(data.srv.ctx, msg, addr2, addrlen2);
			CU_ASSERT_EQUAL(res, 0);

			res = pomp_msg_clear(msg);
			CU_ASSERT_EQUAL(res, 0);
			res = pomp_msg_write(msg, 1, "%s", "hello2->1");
			CU_ASSERT_EQUAL(res, 0);
			res = pomp_ctx_send_msg_to(data.cli.ctx, msg, addr1, addrlen1);
			CU_ASSERT_EQUAL(res, 0);

			/* Invalid send (not connected) */
			res = pomp_ctx_send_msg(data.cli.ctx, msg);
			CU_ASSERT_EQUAL(res, -ENOTCONN);

			/* Invalid send to (NULL param) */
			res = pomp_ctx_send_msg_to(NULL, msg, addr1, addrlen1);
			CU_ASSERT_EQUAL(res, -EINVAL);
			res = pomp_ctx_send_msg_to(data.cli.ctx, NULL, addr1, addrlen1);
			CU_ASSERT_EQUAL(res, -EINVAL);
			res = pomp_ctx_send_msg_to(data.cli.ctx, msg, NULL, addrlen1);
			CU_ASSERT_EQUAL(res, -EINVAL);
		} else {
			buf = pomp_buffer_new(32);
			CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
			memcpy(buf->data, "Hello World !!!", 15);
			buf->len = 15;

			res = pomp_ctx_send_raw_buf_to(data.srv.ctx, buf, addr2, addrlen2);
			CU_ASSERT_EQUAL(res, 0);
			data.datasent += 15;
			res = pomp_ctx_send_raw_buf_to(data.cli.ctx, buf, addr1, addrlen1);
			CU_ASSERT_EQUAL(res, 0);
			data.datasent += 15;

			/* Invalid send (not connected) */
			res = pomp_ctx_send_raw_buf(data.cli.ctx, buf);
			CU_ASSERT_EQUAL(res, -ENOTCONN);

			/* Invalid send to (NULL param) */
			res = pomp_ctx_send_raw_buf_to(NULL, buf, addr1, addrlen1);
			CU_ASSERT_EQUAL(res, -EINVAL);
			res = pomp_ctx_send_raw_buf_to(data.cli.ctx, NULL, addr1, addrlen1);
			CU_ASSERT_EQUAL(res, -EINVAL);
			res = pomp_ctx_send_raw_buf_to(data.cli.ctx, buf, NULL, addrlen1);
			CU_ASSERT_EQUAL(res, -EINVAL);

			pomp_buffer_unref(buf);
			buf = NULL;
		}
	}

	/* Check no send callback directly called by the sending function. */
	CU_ASSERT_EQUAL(data.sendcount, 0);

	/* Run contexts (they shall have answered each other) */
	run_ctx(data.srv.ctx, data.cli.ctx, 100);
	if (!israw) {
		CU_ASSERT_EQUAL(data.msg, 4);
		if (withsendcb)
			CU_ASSERT_EQUAL(data.sendcount, 4);
	} else {
		if (data.isdgram)
			CU_ASSERT_EQUAL(data.buf, 2);
		CU_ASSERT_EQUAL(data.dataread, data.datasent);
		if (withsendcb)
			CU_ASSERT_EQUAL(data.sendcount, 2);
	}

	/* Dummy run */
	res = pomp_ctx_wait_and_process(data.srv.ctx, 100);
	CU_ASSERT_EQUAL(res, -ETIMEDOUT);
	res = pomp_ctx_wait_and_process(NULL, 100);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Wakeup */
	res = pomp_ctx_wakeup(data.srv.ctx);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_ctx_wait_and_process(data.srv.ctx, 100);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid wakeup (NULL param) */
	res = pomp_ctx_wakeup(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Overflow server by writing on client side without running loop */
	if (!isdgram) {
		for (i = 0; i < 1024; i++) {
			if (buf == NULL)
				buf = pomp_buffer_new(1024);
			CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
			for (j = 0; j < 1024; j++)
				buf->data[j] = rand() % 255;
			buf->len = 1024;

			if (!israw) {
				res = pomp_ctx_send(data.cli.ctx, 3, "%p%u", buf->data, 1024);
				CU_ASSERT_EQUAL(res, 0);
			} else {
				res = pomp_ctx_send_raw_buf(data.cli.ctx, buf);
				CU_ASSERT_EQUAL(res, 0);
			}

			if (buf->refcount > 1) {
				pomp_buffer_unref(buf);
				buf = NULL;
			}
		}
		if (buf != NULL) {
			pomp_buffer_unref(buf);
			buf = NULL;
		}

		/* Run contexts (to unlock writes) */
		run_ctx(data.srv.ctx, data.cli.ctx, 100);
		if (!israw)
			CU_ASSERT_EQUAL(data.msg, 4 + 1024);
	}

	/* Recursive send */
	if (withsendcb) {
		/* reset counts */
		data.buf = 0;
		data.msg = 0;
		data.sendcount = 0;
		data.datasent = 0;
		data.dataread = 0;

		/* Enable recursive send. */
		data.srv.recurs_send_enabled = 1;
		send_msg(&data, &data.srv, &data.cli, "srv_to_cli");

		/* Check no send callback directly called by the sending function. */
		CU_ASSERT_EQUAL(data.sendcount, 0);

		run_ctx(data.srv.ctx, data.cli.ctx, 100);

		if (!israw) {
			CU_ASSERT_EQUAL(data.msg, 2);
			CU_ASSERT_EQUAL(data.sendcount, 2);
		} else {
			if (data.isdgram)
				CU_ASSERT_EQUAL(data.buf, 2);
			CU_ASSERT_EQUAL(data.dataread, data.datasent);
			CU_ASSERT_EQUAL(data.sendcount, 2);
		}

		/* Check client recursive send during server disconnection */
		data.cli.recurs_send_enabled = 1;
		send_msg(&data, &data.cli, &data.srv, "cli_to_srv");
	}

	/* Disconnect client from server */
	if (!isdgram) {
		if (withsendcb) {
			/* Check recursive write during disconnection */
			data.srv.recurs_send_enabled = 1;
			data.isdisconnecting = 1;
		}

		res = pomp_conn_disconnect(pomp_ctx_get_next_conn(data.srv.ctx, NULL));
		CU_ASSERT_EQUAL(res, 0);

		/* Check recursive send callback by disconnection */
		if (withsendcb)
			CU_ASSERT_EQUAL(data.sendcount, 2);
	}

	/* Run contexts (they shall disconnect each other) */
	run_ctx(data.srv.ctx, data.cli.ctx, 100);
	pomp_ctx_process_fd(data.cli.ctx);
	if (!isdgram) {
		CU_ASSERT_EQUAL(data.disconnection, 2);

		if (!israw) {
			/* Invalid send (client not connected) */
			res = pomp_ctx_send(data.cli.ctx, 1, "%s", "hello2->1");
			CU_ASSERT_EQUAL(res, -ENOTCONN);
		} else {
			/* TODO */
		}
	}

	/* Invalid destroy (NULL param) */
	res = pomp_ctx_destroy(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid destroy (busy) */
	res = pomp_ctx_destroy(data.srv.ctx);
	CU_ASSERT_EQUAL(res, -EBUSY);

	/* Stop server */
	res = pomp_ctx_stop(data.srv.ctx);
	CU_ASSERT_EQUAL(res, 0);

	/* Stop client */
	res = pomp_ctx_stop(data.cli.ctx);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid stop (NULL param) */
	res = pomp_ctx_stop(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Stop when already done */
	res = pomp_ctx_stop(data.srv.ctx);
	CU_ASSERT_EQUAL(res, 0);

	/* Destroy contexts */
	res = pomp_ctx_destroy(data.srv.ctx);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_ctx_destroy(data.cli.ctx);
	CU_ASSERT_EQUAL(res, 0);

	res = pomp_msg_destroy(msg);
	CU_ASSERT_EQUAL(res, 0);
}

/** */
static void test_ctx_inet_tcp(int israw, int withsockcb, int withsendcb)
{
	struct sockaddr_in addr_in;

	memset(&addr_in, 0, sizeof(addr_in));
	addr_in.sin_family = AF_INET;
	addr_in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr_in.sin_port = htons(5656);

	test_ctx((const struct sockaddr *)&addr_in, sizeof(addr_in),
			(const struct sockaddr *)&addr_in, sizeof(addr_in),
			0, israw, withsockcb, withsendcb);
}

/** */
static void test_ctx_normal_inet_tcp()
{
	test_ctx_inet_tcp(0, 0, 0);
	test_ctx_inet_tcp(0, 1, 0);
	test_ctx_inet_tcp(0, 0, 1);
	test_ctx_inet_tcp(0, 1, 1);
}

/** */
static void test_ctx_raw_inet_tcp()
{
	test_ctx_inet_tcp(1, 0, 0);
	test_ctx_inet_tcp(1, 1, 0);
	test_ctx_inet_tcp(1, 0, 1);
	test_ctx_inet_tcp(1, 1, 1);
}

/** */
static void test_ctx_inet_udp(int israw, int withsockcb, int withsendcb)
{
	struct sockaddr_in addr_in1;
	struct sockaddr_in addr_in2;

	memset(&addr_in1, 0, sizeof(addr_in1));
	addr_in1.sin_family = AF_INET;
	addr_in1.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr_in1.sin_port = htons(5656);

	memset(&addr_in2, 0, sizeof(addr_in2));
	addr_in2.sin_family = AF_INET;
	addr_in2.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr_in2.sin_port = htons(5657);

	test_ctx((const struct sockaddr *)&addr_in1, sizeof(addr_in1),
			(const struct sockaddr *)&addr_in2, sizeof(addr_in2),
			1, israw, withsockcb, withsendcb);
}

/** */
static void test_ctx_normal_inet_udp()
{
	test_ctx_inet_udp(0, 0, 0);
	test_ctx_inet_udp(0, 1, 0);
	test_ctx_inet_udp(0, 0, 1);
	test_ctx_inet_udp(0, 1, 1);
}

/** */
static void test_ctx_raw_inet_udp()
{
	test_ctx_inet_udp(1, 0, 0);
	test_ctx_inet_udp(1, 1, 0);
	test_ctx_inet_udp(1, 0, 1);
	test_ctx_inet_udp(1, 1, 1);
}

#ifndef _WIN32

/** */
static void test_ctx_unix(int israw, int withsockcb, int withsendcb)
{
	struct sockaddr_un addr_un;

	memset(&addr_un, 0, sizeof(addr_un));
	addr_un.sun_family = AF_UNIX;
	strcpy(addr_un.sun_path, "/tmp/tst-pomp");

	test_ctx((const struct sockaddr *)&addr_un, sizeof(addr_un),
			(const struct sockaddr *)&addr_un, sizeof(addr_un),
			0, israw, withsockcb, withsendcb);
}

/** */
static void test_ctx_normal_unix()
{
	test_ctx_unix(0, 0, 0);
	test_ctx_unix(0, 1, 0);
	test_ctx_unix(0, 0, 1);
	test_ctx_unix(0, 1, 1);
}

/** */
static void test_ctx_raw_unix()
{
	test_ctx_unix(1, 0, 0);
	test_ctx_unix(1, 1, 0);
	test_ctx_unix(1, 0, 1);
	test_ctx_unix(1, 1, 1);
}

#endif /* !WIN32 */

/** */
static void test_local_addr(void)
{
	int res = 0;
	struct sockaddr_in addr_in;
	struct pomp_ctx *ctx = NULL;
	const struct sockaddr *addr = NULL;
	uint32_t addrlen = 0;

	/* Create context */
	ctx = pomp_ctx_new(&test_event_cb_t, NULL);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx);

	/* Start as server with known port */
	memset(&addr_in, 0, sizeof(addr_in));
	addr_in.sin_family = AF_INET;
	addr_in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr_in.sin_port = htons(5656);
	res = pomp_ctx_listen(ctx, (const struct sockaddr *)&addr_in,
			sizeof(addr_in));
	CU_ASSERT_EQUAL(res, 0);

	/* Get local address */
	addr = pomp_ctx_get_local_addr(ctx, &addrlen);
	CU_ASSERT_PTR_NOT_NULL(addr);
	CU_ASSERT_EQUAL(addrlen, sizeof(addr_in));
	CU_ASSERT_EQUAL(memcmp(addr, &addr_in, sizeof(addr_in)), 0);

	/* Stop context */
	res = pomp_ctx_stop(ctx);
	CU_ASSERT_EQUAL(res, 0);

	/* Start as server with dynamic port */
	memset(&addr_in, 0, sizeof(addr_in));
	addr_in.sin_family = AF_INET;
	addr_in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr_in.sin_port = 0;
	res = pomp_ctx_listen(ctx, (const struct sockaddr *)&addr_in,
			sizeof(addr_in));
	CU_ASSERT_EQUAL(res, 0);

	/* Get local address */
	addr = pomp_ctx_get_local_addr(ctx, &addrlen);
	CU_ASSERT_PTR_NOT_NULL(addr);
	CU_ASSERT_EQUAL(addrlen, sizeof(addr_in));
	CU_ASSERT_NOT_EQUAL(((const struct sockaddr_in *)addr)->sin_port, 0);

	/* Stop context */
	res = pomp_ctx_stop(ctx);
	CU_ASSERT_EQUAL(res, 0);

	/* Start as dgram with known port */
	memset(&addr_in, 0, sizeof(addr_in));
	addr_in.sin_family = AF_INET;
	addr_in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr_in.sin_port = htons(5656);
	res = pomp_ctx_bind(ctx, (const struct sockaddr *)&addr_in,
			sizeof(addr_in));
	CU_ASSERT_EQUAL(res, 0);

	/* Get local address */
	addr = pomp_ctx_get_local_addr(ctx, &addrlen);
	CU_ASSERT_PTR_NOT_NULL(addr);
	CU_ASSERT_EQUAL(addrlen, sizeof(addr_in));
	CU_ASSERT_EQUAL(memcmp(addr, &addr_in, sizeof(addr_in)), 0);

	/* Stop context */
	res = pomp_ctx_stop(ctx);
	CU_ASSERT_EQUAL(res, 0);

	/* Start as dgram with dynamic port */
	memset(&addr_in, 0, sizeof(addr_in));
	addr_in.sin_family = AF_INET;
	addr_in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr_in.sin_port = 0;
	res = pomp_ctx_bind(ctx, (const struct sockaddr *)&addr_in,
			sizeof(addr_in));
	CU_ASSERT_EQUAL(res, 0);

	/* Get local address */
	addr = pomp_ctx_get_local_addr(ctx, &addrlen);
	CU_ASSERT_PTR_NOT_NULL(addr);
	CU_ASSERT_EQUAL(addrlen, sizeof(addr_in));
	CU_ASSERT_NOT_EQUAL(((const struct sockaddr_in *)addr)->sin_port, 0);

	/* Stop context */
	res = pomp_ctx_stop(ctx);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid get local addr */
	addr = pomp_ctx_get_local_addr(NULL, &addrlen);
	CU_ASSERT_PTR_NULL(addr);
	addr = pomp_ctx_get_local_addr(ctx, NULL);
	CU_ASSERT_PTR_NULL(addr);
	res = pomp_ctx_connect(ctx, (const struct sockaddr *)&addr_in,
			sizeof(addr_in));
	CU_ASSERT_EQUAL(res, 0);
	addr = pomp_ctx_get_local_addr(ctx, &addrlen);
	CU_ASSERT_PTR_NULL(addr);
	res = pomp_ctx_stop(ctx);
	CU_ASSERT_EQUAL(res, 0);

	/* Free context */
	res = pomp_ctx_destroy(ctx);
	CU_ASSERT_EQUAL(res, 0);
}

/** */
static void test_invalid_addr(void)
{
	int res = 0;
	struct test_data data;
	struct sockaddr_in addr_in;
	struct sockaddr *addr = NULL;
	uint32_t addrlen = 0;
	struct pomp_ctx *ctx = NULL;

	memset(&data, 0, sizeof(data));

	/* Setup test address (inexistent addr) */
	memset(&addr_in, 0, sizeof(addr_in));
	addr_in.sin_family = AF_INET;
	addr_in.sin_addr.s_addr = 0x01000001;
	addr_in.sin_port = htons(5656);
	addr = (struct sockaddr *)&addr_in;
	addrlen = sizeof(addr_in);

	/* Create context */
	ctx = pomp_ctx_new(&test_event_cb_t, &data);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx);

	/* Start as server */
	res = pomp_ctx_listen(ctx, addr, addrlen);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_ctx_wait_and_process(ctx, 2500);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_ctx_stop(ctx);
	CU_ASSERT_EQUAL(res, 0);

	/* Start as client */
	res = pomp_ctx_connect(ctx, addr, addrlen);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_ctx_wait_and_process(ctx, 2500);
	CU_ASSERT(res == 0 || res == -ETIMEDOUT);
	res = pomp_ctx_stop(ctx);
	CU_ASSERT_EQUAL(res, 0);

	/* Destroy context */
	res = pomp_ctx_destroy(ctx);
	CU_ASSERT_EQUAL(res, 0);

}

/* Disable some gcc warnings for test suite descriptions */
#ifdef __GNUC__
#  pragma GCC diagnostic ignored "-Wcast-qual"
#endif /* __GNUC__ */

/** */
static CU_TestInfo s_ctx_tests[] = {
	{(char *)"ctx_normal_inet_tcp", &test_ctx_normal_inet_tcp},
	{(char *)"ctx_raw_inet_tcp", &test_ctx_raw_inet_tcp},
	{(char *)"ctx_normal_inet_udp", &test_ctx_normal_inet_udp},
	{(char *)"ctx_raw_inet_udp", &test_ctx_raw_inet_udp},
#ifndef _WIN32
	{(char *)"ctx_normal_unix", &test_ctx_normal_unix},
	{(char *)"ctx_raw_unix", &test_ctx_raw_unix},
#endif /* !_WIN32 */
	{(char *)"ctx_local_addr", &test_local_addr},
	{(char *)"ctx_invalid_addr", &test_invalid_addr},
	CU_TEST_INFO_NULL,
};

/** */
/*extern*/ CU_SuiteInfo g_suites_ctx[] = {
	{(char *)"ctx", NULL, NULL, s_ctx_tests},
	CU_SUITE_INFO_NULL,
};
