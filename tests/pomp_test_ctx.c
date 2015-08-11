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

#ifndef WIN32

/** */
struct test_data {
	uint32_t  connection;
	uint32_t  disconnection;
	uint32_t  msg;
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
	const struct ucred *cred = NULL;

	switch (event) {
	case POMP_EVENT_CONNECTED:
		data->connection++;

		addr = pomp_conn_get_local_addr(conn, &addrlen);
		CU_ASSERT_TRUE(addr != NULL);
		addr = pomp_conn_get_peer_addr(conn, &addrlen);
		CU_ASSERT_TRUE(addr != NULL);

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
		cred = pomp_conn_get_peer_cred(conn);
		CU_ASSERT_TRUE(cred == NULL);
		cred = pomp_conn_get_peer_cred(NULL);
		CU_ASSERT_TRUE(cred == NULL);
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
static void run_ctx(struct pomp_ctx *ctx1, struct pomp_ctx *ctx2, uint32_t timeout)
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

/** */
static void test_ctx(void)
{
	int res = 0;
	struct test_data data;
	struct sockaddr_in addr_in;
	struct sockaddr *addr = NULL;
	uint32_t addrlen = 0;
	struct pomp_ctx *ctx1 = NULL;
	struct pomp_ctx *ctx2 = NULL;
	struct pomp_loop *loop = NULL;
	struct pomp_conn *conn = NULL;
	int fd = -1;
	uint32_t i = 0;
	void *buf = NULL;

	memset(&data, 0, sizeof(data));

	/* Setup test address */
	memset(&addr_in, 0, sizeof(addr_in));
	addr_in.sin_family = AF_INET;
	addr_in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr_in.sin_port = htons(5656);
	addr = (struct sockaddr *)&addr_in;
	addrlen = sizeof(addr_in);

	/* Create context */
	ctx1 = pomp_ctx_new(&test_event_cb_t, &data);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx1);

	/* Invalid create (NULL param) */
	ctx2 = pomp_ctx_new(NULL, &data);
	CU_ASSERT_PTR_NULL(ctx2);

	/* Create 2nd context */
	ctx2 = pomp_ctx_new(&test_event_cb_t, &data);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx2);

	/* Invalid start server (NULL param) */
	res = pomp_ctx_listen(NULL, addr, addrlen);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_ctx_listen(ctx1, NULL, addrlen);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Start as server 1st context */
	res = pomp_ctx_listen(ctx1, addr, addrlen);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid start server (busy) */
	res = pomp_ctx_listen(ctx1, addr, addrlen);
	CU_ASSERT_EQUAL(res, -EBUSY);

	/* Invalid start client (NULL param) */
	res = pomp_ctx_connect(NULL, addr, addrlen);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_ctx_connect(ctx2, NULL, addrlen);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Start as client 2nd context */
	res = pomp_ctx_connect(ctx2, addr, addrlen);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid start client (busy) */
	res = pomp_ctx_connect(ctx2, addr, addrlen);
	CU_ASSERT_EQUAL(res, -EBUSY);

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
	CU_ASSERT_TRUE(fd >= 0);

	/* Invalid process fd (NULL param) */
	res = pomp_ctx_process_fd(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Run contexts (they shall connect each other) */
	run_ctx(ctx1, ctx2, 100);
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

	/* Exchange some message */
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

	/* Run contexts (they shall have answered each other) */
	run_ctx(ctx1, ctx2, 100);
	CU_ASSERT_EQUAL(data.msg, 4);

	/* Dummy run */
	res = pomp_ctx_wait_and_process(ctx1, 100);
	CU_ASSERT_EQUAL(res, -ETIMEDOUT);
	res = pomp_ctx_wait_and_process(NULL, 100);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Overflow server by writing on client side without running loop */
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

	/* Disconnect client from server */
	res = pomp_conn_disconnect(pomp_ctx_get_next_conn(ctx1, NULL));
	CU_ASSERT_EQUAL(res, 0);

	/* Run contexts (they shall disconnect each other) */
	run_ctx(ctx1, ctx2, 100);
	CU_ASSERT_EQUAL(data.disconnection, 2);

	/* Invalid send (client not connected) */
	res = pomp_ctx_send(ctx2, 1, "%s", "hello2->1");
	CU_ASSERT_EQUAL(res, -ENOTCONN);

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
	{(char *)"ctx", &test_ctx},
	{(char *)"ctx_invalid_addr", &test_invalid_addr},
	CU_TEST_INFO_NULL,
};

/** */
/*extern*/ CU_SuiteInfo g_suites_ctx[] = {
	{(char *)"ctx", NULL, NULL, s_ctx_tests},
	CU_SUITE_INFO_NULL,
};

#endif /* !_WIN32 */
