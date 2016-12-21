/**
 * @file pomp_test_ipc.c
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

#ifndef WIN32

#define ADDR_UNIX		"/tmp/pomp_test.socket"
#define ADDR_UNIX_ABSTRACT	"@/tmp/pomp_test.socket"

#define TEST_IPC_LOG(fmt, ...) \
	fprintf(stderr, "pid %4d: " fmt "\n", getpid(), ##__VA_ARGS__); \

#define TEST_IPC_CHECK(data, res) \
	do { \
		if (!(res)) { \
			(data)->status = -1; \
			TEST_IPC_LOG("  IPC check failure: %s:%d (%s)", \
					__FILE__, __LINE__, __func__); \
		} \
	} while (0)

#define TEST_IPC_CHECK_EQUAL(data, actual, expected) \
	TEST_IPC_CHECK(data, (actual) == (expected))

#define TEST_IPC_CHECK_STRING_EQUAL(data, actual, expected) \
	TEST_IPC_CHECK(data, strcmp(actual, expected) == 0)

/** */
struct test_data {
	int	status;
	int	stop;
	pid_t	pid;
};

/** */
static void test_ipc_timer_cb(struct pomp_timer *timer, void *userdata)
{
	struct test_data *data = userdata;
	/* Timeout during test, force failure and exit */
	data->status = -1;
	data->stop = 1;
}

/** */
static int test_ipc_entry(pomp_event_cb_t cb,
		const struct sockaddr *addr, uint32_t addrlen, int client)
{
	int res = 0;
	struct test_data data;
	struct pomp_ctx *ctx = NULL;
	struct pomp_loop *loop = NULL;
	struct pomp_timer *timer = NULL;

	memset(&data, 0, sizeof(data));
	data.pid = getpid();

	TEST_IPC_LOG("-> %s", __func__);

	/* Setup */
	ctx = pomp_ctx_new(cb, &data);
	TEST_IPC_CHECK(&data, ctx != NULL);
	loop = pomp_ctx_get_loop(ctx);
	TEST_IPC_CHECK(&data, loop != NULL);
	timer = pomp_timer_new(loop, &test_ipc_timer_cb, &data);
	TEST_IPC_CHECK(&data, timer != NULL);
	res = pomp_timer_set(timer, 10 * 1000);
	TEST_IPC_CHECK(&data, res == 0);

	/* Start client/server */
	if (client)
		res = pomp_ctx_connect(ctx, addr, addrlen);
	else
		res = pomp_ctx_listen(ctx, addr, addrlen);
	TEST_IPC_CHECK(&data, res == 0);

	/* Run loop */
	while (!data.stop)
		pomp_loop_wait_and_process(loop, -1);

	/* Cleanup */
	res = pomp_timer_destroy(timer);
	TEST_IPC_CHECK(&data, res == 0);
	res = pomp_ctx_stop(ctx);
	TEST_IPC_CHECK(&data, res == 0);
	res = pomp_ctx_destroy(ctx);
	TEST_IPC_CHECK(&data, res == 0);

	TEST_IPC_LOG("<-- %s", __func__);
	return data.status;
}

/** */
static pid_t test_ipc_start(pomp_event_cb_t cb,
		const struct sockaddr *addr, uint32_t addrlen, int client)
{
	pid_t pid = 0;
	pid = fork();
	if (pid < 0) {
		return -1;
	} else if (pid == 0) {
		/* Child */
		exit(test_ipc_entry(cb, addr, addrlen, client));
	} else {
		/* Parent */
		return pid;
	}
}

/** */
static int test_ipc(pomp_event_cb_t servercb, pomp_event_cb_t clientcb,
		const struct sockaddr *addr, uint32_t addrlen)
{
	pid_t server_pid = -1, client_pid = -1;
	int server_status = -1, client_status = -1;
	int server_ok = 0, client_ok = 0;

	if (servercb != NULL) {
		TEST_IPC_LOG("starting server");
		server_pid = test_ipc_start(servercb, addr, addrlen, 1);
	} else {
		server_ok = 1;
	}

	if (clientcb != NULL) {
		TEST_IPC_LOG("starting client");
		client_pid = test_ipc_start(clientcb, addr, addrlen, 0);
	} else {
		client_ok = 1;
	}

	if (server_pid != -1) {
		waitpid(server_pid, &server_status, 0);
		TEST_IPC_LOG("server status: %d", server_status);
		if (WIFEXITED(server_status))
			server_ok = (WEXITSTATUS(server_status) == 0);
	}

	if (client_pid != -1) {
		waitpid(client_pid, &client_status, 0);
		TEST_IPC_LOG("client status: %d", client_status);
		if (WIFEXITED(client_status))
			client_ok = (WEXITSTATUS(client_status) == 0);
	}

	return server_ok && client_ok ? 0 : -1;
}

/** */
static void fill_addr_unix(struct sockaddr *addr, uint32_t *addrlen)
{
	struct sockaddr_un *addr_un = (struct sockaddr_un *)addr;
	memset(addr_un, 0, sizeof(*addr_un));
	addr_un->sun_family = AF_UNIX;
	strncpy(addr_un->sun_path, ADDR_UNIX, sizeof(addr_un->sun_path));
	*addrlen = sizeof(*addr_un);
}

/** */
static void test_fd_passing_server(struct pomp_ctx *ctx,
		enum pomp_event event,
		struct pomp_conn *conn,
		const struct pomp_msg *msg,
		void *userdata)
{
	static int fds[3][2] = {{-1, -1}, {-1, -1}, {-1, -1}};
	int res = 0;
	struct test_data *data = userdata;

	TEST_IPC_LOG("%s : event=%d(%s) conn=%p msg=%p", __func__,
			event, pomp_event_str(event), conn, msg);

	switch (event) {
	case POMP_EVENT_CONNECTED:
		/* Create 3 pairs of pipes */
		res = pipe(fds[0]); TEST_IPC_CHECK_EQUAL(data, res, 0);
		res = pipe(fds[1]); TEST_IPC_CHECK_EQUAL(data, res, 0);
		res = pipe(fds[2]); TEST_IPC_CHECK_EQUAL(data, res, 0);

		/* Send read sides to client */
		res = pomp_conn_send(conn, 1, "%s%x%s%x%s%x",
				"pipe0", fds[0][0],
				"pipe1", fds[1][0],
				"pipe2", fds[2][0]);
		TEST_IPC_CHECK_EQUAL(data, res, 0);

		/* write some data on pipes */
		res = (int)write(fds[0][1], "pipe0", 5);
		TEST_IPC_CHECK_EQUAL(data, res, 5);
		res = (int)write(fds[1][1], "pipe1", 5);
		TEST_IPC_CHECK_EQUAL(data, res, 5);
		res = (int)write(fds[2][1], "pipe2", 5);
		TEST_IPC_CHECK_EQUAL(data, res, 5);
		break;

	case POMP_EVENT_DISCONNECTED:
		/* Close all pipes */
		res = close(fds[0][0]); TEST_IPC_CHECK_EQUAL(data, res, 0);
		res = close(fds[0][1]); TEST_IPC_CHECK_EQUAL(data, res, 0);
		res = close(fds[1][0]); TEST_IPC_CHECK_EQUAL(data, res, 0);
		res = close(fds[1][1]); TEST_IPC_CHECK_EQUAL(data, res, 0);
		res = close(fds[2][0]); TEST_IPC_CHECK_EQUAL(data, res, 0);
		res = close(fds[2][1]); TEST_IPC_CHECK_EQUAL(data, res, 0);

		data->stop = 1;
		break;

	case POMP_EVENT_MSG:
		break;

	default:
		break;
	}
}

/** */
static void test_fd_passing_client(struct pomp_ctx *ctx,
		enum pomp_event event,
		struct pomp_conn *conn,
		const struct pomp_msg *msg,
		void *userdata)
{
	int res = 0;
	int fds[3] = {-1, -1, -1};
	char *str0 = NULL, *str1 = NULL, *str2 = NULL;
	char buf[32] = "";
	struct test_data *data = userdata;

	TEST_IPC_LOG("%s : event=%d(%s) conn=%p msg=%p", __func__,
			event, pomp_event_str(event), conn, msg);

	switch (event) {
	case POMP_EVENT_CONNECTED:
		break;

	case POMP_EVENT_DISCONNECTED:
		data->stop = 1;
		break;

	case POMP_EVENT_MSG:
		if (pomp_msg_get_id(msg) == 1) {
			res = pomp_msg_read(msg, "%ms%x%ms%x%ms%x",
					&str0, &fds[0],
					&str1, &fds[1],
					&str2, &fds[2]);
			TEST_IPC_CHECK_EQUAL(data, res, 0);

			memset(buf, 0, sizeof(buf));
			res = (int)read(fds[0], buf, sizeof(buf) - 1);
			TEST_IPC_CHECK_EQUAL(data, res, 5);
			TEST_IPC_CHECK_STRING_EQUAL(data, str0, "pipe0");
			TEST_IPC_CHECK_STRING_EQUAL(data, buf, "pipe0");

			memset(buf, 0, sizeof(buf));
			res = (int)read(fds[1], buf, sizeof(buf) - 1);
			TEST_IPC_CHECK_EQUAL(data, res, 5);
			TEST_IPC_CHECK_STRING_EQUAL(data, str1, "pipe1");
			TEST_IPC_CHECK_STRING_EQUAL(data, buf, "pipe1");

			memset(buf, 0, sizeof(buf));
			res = (int)read(fds[2], buf, sizeof(buf) - 1);
			TEST_IPC_CHECK_EQUAL(data, res, 5);
			TEST_IPC_CHECK_STRING_EQUAL(data, str2, "pipe2");
			TEST_IPC_CHECK_STRING_EQUAL(data, buf, "pipe2");

			free(str0);
			free(str1);
			free(str2);

			pomp_conn_disconnect(conn);
		}

		break;

	default:
		break;
	}
}

/** */
static void test_fd_passing(void)
{
	int res = 0;
	struct sockaddr_storage addr_storage;
	struct sockaddr *addr = NULL;
	uint32_t addrlen = 0;

	memset(&addr_storage, 0, sizeof(addr_storage));
	addr = (struct sockaddr *)&addr_storage;
	addrlen = sizeof(addr_storage);
	fill_addr_unix(addr, &addrlen);

	res = test_ipc(&test_fd_passing_server,
			&test_fd_passing_client,
			addr, addrlen);
	CU_ASSERT_EQUAL(res, 0);
}

/* Disable some gcc warnings for test suite descriptions */
#ifdef __GNUC__
#  pragma GCC diagnostic ignored "-Wcast-qual"
#endif /* __GNUC__ */

/** */
static CU_TestInfo s_fd_passing_tests[] = {
	{(char *)"fd_passing", &test_fd_passing},
	CU_TEST_INFO_NULL,
};

/** */
/*extern*/ CU_SuiteInfo g_suites_ipc[] = {
	{(char *)"fd_passing", NULL, NULL, s_fd_passing_tests},
	CU_SUITE_INFO_NULL,
};

#endif /* !_WIN32 */
