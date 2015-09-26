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

#include "pomp_test.h"

/** */
struct test_data {
	uint32_t  connection;
	uint32_t  disconnection;
	uint32_t  msg;
	int       isdgram;
};

/** */
static void test_event_cb_t(struct pomp_ctx *ctx, enum pomp_event event,
		struct pomp_conn *conn, const struct pomp_msg *msg,
		void *userdata)
{
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

		/* Invalid get credentials (bad type or NULL param) */
		if (!isunix) {
			cred = pomp_conn_get_peer_cred(conn);
			CU_ASSERT_TRUE(cred == NULL);
			cred = pomp_conn_get_peer_cred(NULL);
			CU_ASSERT_TRUE(cred == NULL);
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
			res = pomp_conn_send_msg_to(NULL, msg, addr, addrlen);
			CU_ASSERT_EQUAL(res, -EINVAL);
			res = pomp_conn_send_msg_to(conn, NULL, addr, addrlen);
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

#ifndef _WIN32

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

#else /* _WIN32 */

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
		int isdgram)
{
	int res = 0;
	struct test_data data;
	struct pomp_ctx *ctx1 = NULL;
	struct pomp_ctx *ctx2 = NULL;
	struct pomp_loop *loop = NULL;
	struct pomp_conn *conn = NULL;
	struct pomp_msg *msg = NULL;
	int fd = -1;
	uint32_t i = 0;
	void *buf = NULL;

	memset(&data, 0, sizeof(data));
	data.isdgram = isdgram;
	msg = pomp_msg_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(msg);

	/* Create context */
	ctx1 = pomp_ctx_new(&test_event_cb_t, &data);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx1);

	/* Invalid create (NULL param) */
	ctx2 = pomp_ctx_new(NULL, &data);
	CU_ASSERT_PTR_NULL(ctx2);
	ctx2 = pomp_ctx_new_with_loop(NULL, &data, NULL);
	CU_ASSERT_PTR_NULL(ctx2);
	ctx2 = pomp_ctx_new_with_loop(&test_event_cb_t, &data, NULL);
	CU_ASSERT_PTR_NULL(ctx2);

	/* Create 2nd context */
	ctx2 = pomp_ctx_new(&test_event_cb_t, &data);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx2);

	if (!isdgram) {
		/* Invalid start server (NULL param) */
		res = pomp_ctx_listen(NULL, addr1, addrlen1);
		CU_ASSERT_EQUAL(res, -EINVAL);
		res = pomp_ctx_listen(ctx1, NULL, addrlen1);
		CU_ASSERT_EQUAL(res, -EINVAL);

		/* Start as server 1st context */
		res = pomp_ctx_listen(ctx1, addr1, addrlen1);
		CU_ASSERT_EQUAL(res, 0);

		/* Invalid start server (busy) */
		res = pomp_ctx_listen(ctx1, addr1, addrlen1);
		CU_ASSERT_EQUAL(res, -EBUSY);
	} else {
		/* Invalid bind (NULL param) */
		res = pomp_ctx_bind(NULL, addr1, addrlen1);
		CU_ASSERT_EQUAL(res, -EINVAL);
		res = pomp_ctx_bind(ctx1, NULL, addrlen1);
		CU_ASSERT_EQUAL(res, -EINVAL);

		/* Bind 1st context */
		res = pomp_ctx_bind(ctx1, addr1, addrlen1);
		CU_ASSERT_EQUAL(res, 0);

		/* Invalid bind (busy) */
		res = pomp_ctx_bind(ctx1, addr1, addrlen1);
		CU_ASSERT_EQUAL(res, -EBUSY);
	}

	if (!isdgram) {
		/* Invalid start client (NULL param) */
		res = pomp_ctx_connect(NULL, addr2, addrlen2);
		CU_ASSERT_EQUAL(res, -EINVAL);
		res = pomp_ctx_connect(ctx2, NULL, addrlen2);
		CU_ASSERT_EQUAL(res, -EINVAL);

		/* Start as client 2nd context */
		res = pomp_ctx_connect(ctx2, addr2, addrlen2);
		CU_ASSERT_EQUAL(res, 0);

		/* Invalid start client (busy) */
		res = pomp_ctx_connect(ctx2, addr2, addrlen2);
		CU_ASSERT_EQUAL(res, -EBUSY);
	} else {
		/* Invalid bind (NULL param) */
		res = pomp_ctx_bind(NULL, addr2, addrlen2);
		CU_ASSERT_EQUAL(res, -EINVAL);
		res = pomp_ctx_bind(ctx2, NULL, addrlen2);
		CU_ASSERT_EQUAL(res, -EINVAL);

		/* Bind 2nd context */
		res = pomp_ctx_bind(ctx2, addr2, addrlen2);
		CU_ASSERT_EQUAL(res, 0);

		/* Invalid bind (busy) */
		res = pomp_ctx_bind(ctx2, addr2, addrlen2);
		CU_ASSERT_EQUAL(res, -EBUSY);
	}

	/* Invalid get loop (NULL param) */
	loop = pomp_ctx_get_loop(NULL);
	CU_ASSERT_PTR_NULL(loop);

	/* Invalid get fd (NULL param) */
	fd = pomp_ctx_get_fd(NULL);
	CU_ASSERT_EQUAL(fd, -EINVAL);

	/* Get loop and fd */
	loop = pomp_ctx_get_loop(ctx1);
	CU_ASSERT_PTR_NOT_NULL(loop);
	fd = pomp_ctx_get_fd(ctx1);
#ifdef POMP_HAVE_LOOP_EPOLL
	CU_ASSERT_TRUE(fd >= 0);
#else
	CU_ASSERT_EQUAL(fd, -ENOSYS);
#endif
	/* Invalid process fd (NULL param) */
	res = pomp_ctx_process_fd(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Run contexts (they shall connect each other) */
	run_ctx(ctx1, ctx2, 100);
	if (!isdgram) {
		CU_ASSERT_EQUAL(data.connection, 2);

		/* Get remote connections */
		conn = pomp_ctx_get_next_conn(ctx1, NULL);
		CU_ASSERT_PTR_NOT_NULL(conn);
		conn = pomp_ctx_get_next_conn(ctx1, conn);
		CU_ASSERT_PTR_NULL(conn);
		conn = pomp_ctx_get_conn(ctx2);
		CU_ASSERT_PTR_NOT_NULL(conn);

		/* Invalid get remote connections */
		conn = pomp_ctx_get_next_conn(ctx2, NULL);
		CU_ASSERT_PTR_NULL(conn);
		conn = pomp_ctx_get_conn(ctx1);
		CU_ASSERT_PTR_NULL(conn);
		conn = pomp_ctx_get_next_conn(NULL, NULL);
		CU_ASSERT_PTR_NULL(conn);
		conn = pomp_ctx_get_conn(NULL);
		CU_ASSERT_PTR_NULL(conn);
	}

	/* Exchange some message */
	if (!isdgram) {
		res = pomp_ctx_send(ctx1, 1, "%s", "hello1->2");
		CU_ASSERT_EQUAL(res, 0);
		res = pomp_ctx_send(ctx2, 1, "%s", "hello2->1");
		CU_ASSERT_EQUAL(res, 0);

		/* Invalid send (NULL param) */
		res = pomp_ctx_send(NULL, 1, "%s", "hello1->2");
		CU_ASSERT_EQUAL(res, -EINVAL);
		res = pomp_ctx_send_msg(ctx1, NULL);
		CU_ASSERT_EQUAL(res, -EINVAL);

		/* Invalid send (bad format) */
		res = pomp_ctx_send(ctx1, 1, "%o", 1);
		CU_ASSERT_EQUAL(res, -EINVAL);
		res = pomp_conn_send(pomp_ctx_get_conn(ctx2), 1, "%o", 1);
		CU_ASSERT_EQUAL(res, -EINVAL);

		/* Invalid send to (bad type) */
		res = pomp_ctx_send_msg_to(ctx2, msg, addr1, addrlen1);
		CU_ASSERT_EQUAL(res, -EINVAL);
	} else {
		res = pomp_msg_clear(msg);
		CU_ASSERT_EQUAL(res, 0);
		res = pomp_msg_write(msg, 1, "%s", "hello1->2");
		CU_ASSERT_EQUAL(res, 0);
		res = pomp_ctx_send_msg_to(ctx1, msg, addr2, addrlen2);
		CU_ASSERT_EQUAL(res, 0);

		res = pomp_msg_clear(msg);
		CU_ASSERT_EQUAL(res, 0);
		res = pomp_msg_write(msg, 1, "%s", "hello2->1");
		CU_ASSERT_EQUAL(res, 0);
		res = pomp_ctx_send_msg_to(ctx2, msg, addr1, addrlen1);
		CU_ASSERT_EQUAL(res, 0);

		/* Invalid send (not connected) */
		res = pomp_ctx_send_msg(ctx2, msg);
		CU_ASSERT_EQUAL(res, -ENOTCONN);

		/* Invalid send to (NULL param) */
		res = pomp_ctx_send_msg_to(NULL, msg, addr1, addrlen1);
		CU_ASSERT_EQUAL(res, -EINVAL);
		res = pomp_ctx_send_msg_to(ctx2, NULL, addr1, addrlen1);
		CU_ASSERT_EQUAL(res, -EINVAL);
		res = pomp_ctx_send_msg_to(ctx2, msg, NULL, addrlen1);
		CU_ASSERT_EQUAL(res, -EINVAL);
	}

	/* Run contexts (they shall have answered each other) */
	run_ctx(ctx1, ctx2, 100);
	CU_ASSERT_EQUAL(data.msg, 4);

	/* Dummy run */
	res = pomp_ctx_wait_and_process(ctx1, 100);
	CU_ASSERT_EQUAL(res, -ETIMEDOUT);
	res = pomp_ctx_wait_and_process(NULL, 100);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Wakeup */
	res = pomp_ctx_wakeup(ctx1);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_ctx_wait_and_process(ctx1, 100);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid wakeup (NULL param) */
	res = pomp_ctx_wakeup(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Overflow server by writing on client side without running loop */
	if (!isdgram) {
		buf = calloc(1, 1024);
		CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
		for (i = 0; i < 1024; i++) {
			res = pomp_ctx_send(ctx2, 3, "%p%u", buf, 1024);
			CU_ASSERT_EQUAL(res, 0);
		}
		free(buf);

		/* Run contexts (to unlock writes) */
		run_ctx(ctx1, ctx2, 1000);
		CU_ASSERT_EQUAL(data.msg, 4 + 1024);
	}

	/* Disconnect client from server */
	if (!isdgram) {
		res = pomp_conn_disconnect(pomp_ctx_get_next_conn(ctx1, NULL));
		CU_ASSERT_EQUAL(res, 0);
	}

	/* Run contexts (they shall disconnect each other) */
	run_ctx(ctx1, ctx2, 100);
	if (!isdgram) {
		CU_ASSERT_EQUAL(data.disconnection, 2);

		/* Invalid send (client not connected) */
		res = pomp_ctx_send(ctx2, 1, "%s", "hello2->1");
		CU_ASSERT_EQUAL(res, -ENOTCONN);
	}

	/* Invalid destroy (NULL param) */
	res = pomp_ctx_destroy(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid destroy (busy) */
	res = pomp_ctx_destroy(ctx1);
	CU_ASSERT_EQUAL(res, -EBUSY);

	/* Stop server */
	res = pomp_ctx_stop(ctx1);
	CU_ASSERT_EQUAL(res, 0);

	/* Stop client */
	res = pomp_ctx_stop(ctx2);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid stop (NULL param) */
	res = pomp_ctx_stop(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid stop (already done) */
	res = pomp_ctx_stop(ctx1);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Destroy contexts */
	res = pomp_ctx_destroy(ctx1);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_ctx_destroy(ctx2);
	CU_ASSERT_EQUAL(res, 0);

	res = pomp_msg_destroy(msg);
	CU_ASSERT_EQUAL(res, 0);
}

/** */
static void test_ctx_inet_tcp(void)
{
	struct sockaddr_in addr_in;

	memset(&addr_in, 0, sizeof(addr_in));
	addr_in.sin_family = AF_INET;
	addr_in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr_in.sin_port = htons(5656);

	test_ctx((const struct sockaddr *)&addr_in, sizeof(addr_in),
			(const struct sockaddr *)&addr_in, sizeof(addr_in), 0);
}

/** */
static void test_ctx_inet_udp(void)
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
			(const struct sockaddr *)&addr_in2, sizeof(addr_in2), 1);
}

/** */
static void test_ctx_unix(void)
{
	struct sockaddr_un addr_un;

	memset(&addr_un, 0, sizeof(addr_un));
	addr_un.sun_family = AF_UNIX;
	strcpy(addr_un.sun_path, "/tmp/tst-pomp");

	test_ctx((const struct sockaddr *)&addr_un, sizeof(addr_un),
			(const struct sockaddr *)&addr_un, sizeof(addr_un), 0);
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
	CU_ASSERT_EQUAL(res, 0);
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
	{(char *)"ctx_inet_tcp", &test_ctx_inet_tcp},
	{(char *)"ctx_inet_udp", &test_ctx_inet_udp},
	{(char *)"ctx_unix", &test_ctx_unix},
	{(char *)"ctx_invalid_addr", &test_invalid_addr},
	CU_TEST_INFO_NULL,
};

/** */
/*extern*/ CU_SuiteInfo g_suites_ctx[] = {
	{(char *)"ctx", NULL, NULL, s_ctx_tests},
	CU_SUITE_INFO_NULL,
};
