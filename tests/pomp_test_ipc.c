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
	int	flags;
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

static int write_with_extra_fds(struct pomp_conn *conn,
		const void *buf, size_t len, int* fds, int nfds)
{
	struct iovec iov;
	struct msghdr msg;
	struct cmsghdr *cmsg = NULL;
	uint8_t cmsg_buf[CMSG_SPACE(4 * sizeof(int))];
	ssize_t writelen = 0;
	int i = 0;

	memset(&msg, 0, sizeof(msg));
	memset(&cmsg_buf, 0, sizeof(cmsg_buf));

	/* Setup the data part of the socket message */
	iov.iov_base = (void *)buf;
	iov.iov_len = len;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	/* Setup the control part of the socket message */
	msg.msg_control = cmsg_buf;
	msg.msg_controllen = CMSG_SPACE(nfds * sizeof(int));
	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(nfds * sizeof(int));

	/* Copy file descriptors */
	for (i = 0; i < nfds; i++)
		((int *)CMSG_DATA(cmsg))[i] = fds[i];

	/* Write data ignoring interrupts */
	do {
		writelen = sendmsg(pomp_conn_get_fd(conn), &msg, 0);
	} while (writelen < 0 && errno == EINTR);

	return writelen == (ssize_t)len ? 0 : -EIO;
}

/** */
static void test_fd_passing_server(struct pomp_ctx *ctx,
		enum pomp_event event,
		struct pomp_conn *conn,
		const struct pomp_msg *msg,
		void *userdata)
{
#define PIPE_COUNT 10
	static int fds_msg0[3][2] = {{-1, -1}, {-1, -1}, {-1, -1}};
	static int fds_msg2[2][2] = {{-1, -1}, {-1, -1}};
	static int fds_msg3[2][2] = {{-1, -1}, {-1, -1}};
	static int fds_msg5[3][2] = {{-1, -1}, {-1, -1}, {-1, -1}};
	static int *fds[PIPE_COUNT] = {
		fds_msg0[0], fds_msg0[1], fds_msg0[2],
		fds_msg2[0], fds_msg2[1],
		fds_msg3[0], fds_msg3[1],
		fds_msg5[0], fds_msg5[1], fds_msg5[2],
	};

	static uint8_t dummy_buf[3 * POMP_CONN_READ_SIZE];
	int res = 0;
	struct test_data *data = userdata;
	uint32_t bufsz = 0;
	uint32_t i = 0;
	size_t chunk_off = 0;
	struct pomp_msg *msg2 = NULL;

	TEST_IPC_LOG("%s : event=%d(%s) conn=%p msg=%p", __func__,
			event, pomp_event_str(event), conn, msg);

	switch (event) {
	case POMP_EVENT_CONNECTED:
		/* Create pairs of pipes */
		for (i = 0; i < PIPE_COUNT; i++) {
			res = pipe(fds[i]);
			TEST_IPC_CHECK_EQUAL(data, res, 0);
		}

		/* write some data on pipes */
		for (i = 0; i < PIPE_COUNT; i++) {
			char buf[32] = "";
			snprintf(buf, sizeof(buf), "pipe%d", i);
			res = (int)write(fds[i][1], buf, strlen(buf));
			TEST_IPC_CHECK_EQUAL(data, res, (int)strlen(buf));
		}

		/* Send read sides to client */
		res = pomp_conn_send(conn, 0, "%s%x%s%x%s%x",
				"pipe0", fds_msg0[0][0],
				"pipe1", fds_msg0[1][0],
				"pipe2", fds_msg0[2][0]);
		TEST_IPC_CHECK_EQUAL(data, res, 0);

		/* Send a properly formatted message with 0 fd but with 1 fd in ancillary data */
		msg2 = pomp_msg_new();
		res = pomp_msg_write(msg2, 100, "%d", 100);
		TEST_IPC_CHECK_EQUAL(data, res, 0);
		res = write_with_extra_fds(conn,
				msg2->buf->data, msg2->buf->len,
				(int[1]){fds_msg0[0][0]}, 1);
		TEST_IPC_CHECK_EQUAL(data, res, 0);
		res = pomp_msg_destroy(msg2);
		TEST_IPC_CHECK_EQUAL(data, res, 0);

		/* Send a properly formatted message with 1 fd but with 2 fds in ancillary data */
		msg2 = pomp_msg_new();
		res = pomp_msg_write(msg2, 101, "%d%x", 101, fds_msg0[0][0]);
		TEST_IPC_CHECK_EQUAL(data, res, 0);
		res = write_with_extra_fds(conn,
				msg2->buf->data, msg2->buf->len,
				(int[2]){fds_msg0[0][0], fds_msg0[1][0]}, 2);
		TEST_IPC_CHECK_EQUAL(data, res, 0);
		res = pomp_msg_destroy(msg2);
		TEST_IPC_CHECK_EQUAL(data, res, 0);

		/* Send a properly formatted message with 2 fds but with 1 fd in ancillary data */
		msg2 = pomp_msg_new();
		res = pomp_msg_write(msg2, 102, "%d%x%x", 102, fds_msg0[0][0], fds_msg0[1][0]);
		TEST_IPC_CHECK_EQUAL(data, res, 0);
		res = write_with_extra_fds(conn,
				msg2->buf->data, msg2->buf->len,
				(int[1]){fds_msg0[0][0]}, 1);
		TEST_IPC_CHECK_EQUAL(data, res, 0);
		res = pomp_msg_destroy(msg2);
		TEST_IPC_CHECK_EQUAL(data, res, 0);

		/* Send a properly formatted message with 0 fd but with 1 fd in ancillary data of each chunk */
		bufsz = 3 * POMP_CONN_READ_SIZE;
		msg2 = pomp_msg_new();
		res = pomp_msg_write(msg2, 103, "%p%u", dummy_buf, bufsz);
		TEST_IPC_CHECK_EQUAL(data, res, 0);

		chunk_off = 0;
		while (chunk_off < msg2->buf->len) {
			size_t chunk_len = POMP_CONN_READ_SIZE;
			if (chunk_len > msg2->buf->len - chunk_off)
				chunk_len = msg2->buf->len - chunk_off;
			res = write_with_extra_fds(conn,
					msg2->buf->data + chunk_off,
					chunk_len,
					(int[1]){fds_msg0[0][0]}, 1);
			TEST_IPC_CHECK_EQUAL(data, res, 0);
			chunk_off += chunk_len;
		}

		res = pomp_msg_destroy(msg2);
		TEST_IPC_CHECK_EQUAL(data, res, 0);

		/* Check merge/split of messages with and without ancillary data
		 * described here:
		 * https://unix.stackexchange.com/questions/185011/what-happens-with-unix-stream-ancillary-data-on-partial-reads
		 *
		 * Our read buffer size is 4096 in pomp_conn, sizes may need to
		 * be adjusted in this test if changed
		 *
		 * msg1 [~4000 bytes] (no ancillary data)
		 * msg2 [~1000 bytes] (2 file descriptors)
		 * msg3 [~1000 bytes] (2 file descriptor)
		 * msg4 [~2000 bytes] (no ancillary data)
		 * msg5 [~1000 bytes] (3 file descriptors)
		 *
		 * recv1: [4096 bytes]  (msg1 + partial msg2 with msg2's 2 file descriptors)
		 * recv2: [~1904 bytes] (remainder of msg2 + msg3 with msg3's 2 file descriptors)
		 * recv3: [~3000 bytes] (msg4 + msg5 with msg5's 3 file descriptors)
		 */
		TEST_IPC_CHECK_EQUAL(data, POMP_CONN_READ_SIZE, 4096);

		bufsz = POMP_CONN_READ_SIZE - 100;
		res = pomp_conn_send(conn, 1, "%p%u",
				dummy_buf, bufsz);
		TEST_IPC_CHECK_EQUAL(data, res, 0);

		bufsz = 1000;
		res = pomp_conn_send(conn, 2, "%x%p%u%x",
				fds_msg2[0][0],
				dummy_buf, bufsz,
				fds_msg2[1][0]);
		TEST_IPC_CHECK_EQUAL(data, res, 0);

		bufsz = 1000;
		res = pomp_conn_send(conn, 3, "%x%p%u%x",
				fds_msg3[0][0],
				dummy_buf, bufsz,
				fds_msg3[1][0]);
		TEST_IPC_CHECK_EQUAL(data, res, 0);

		bufsz = 2000;
		res = pomp_conn_send(conn, 4, "%p%u",
				dummy_buf, bufsz);
		TEST_IPC_CHECK_EQUAL(data, res, 0);

		bufsz = 1000;
		res = pomp_conn_send(conn, 5, "%x%p%u%x%x",
				fds_msg5[0][0],
				dummy_buf, bufsz,
				fds_msg5[1][0],
				fds_msg5[2][0]);
		TEST_IPC_CHECK_EQUAL(data, res, 0);

		break;

	case POMP_EVENT_DISCONNECTED:
		/* Close all pipes */
		for (i = 0; i < PIPE_COUNT; i++) {
			res = close(fds[i][0]);
			TEST_IPC_CHECK_EQUAL(data, res, 0);
			res = close(fds[i][1]);
			TEST_IPC_CHECK_EQUAL(data, res, 0);
		}

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
	void *pbuf = NULL;
	uint32_t bufsz = 0;
	struct test_data *data = userdata;

	TEST_IPC_LOG("%s : event=%d(%s) conn=%p msg=%p", __func__,
			event, pomp_event_str(event), conn, msg);

	switch (event) {
	case POMP_EVENT_CONNECTED:
		break;

	case POMP_EVENT_DISCONNECTED:
		if (data->flags != 0x3f)
			data->status = -1;
		data->stop = 1;
		break;

	case POMP_EVENT_MSG:
		if (pomp_msg_get_id(msg) == 0) {
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

			data->flags |= 1 << pomp_msg_get_id(msg);
		} else if (pomp_msg_get_id(msg) == 1) {
			res = pomp_msg_read(msg, "%p%u", &pbuf, &bufsz);
			TEST_IPC_CHECK_EQUAL(data, res, 0);
			TEST_IPC_CHECK_EQUAL(data, bufsz, POMP_CONN_READ_SIZE - 100);

			data->flags |= 1 << pomp_msg_get_id(msg);
		} else if (pomp_msg_get_id(msg) == 2) {
			res = pomp_msg_read(msg, "%x%p%u%x",
					&fds[0],
					&pbuf, &bufsz,
					&fds[1]);
			TEST_IPC_CHECK_EQUAL(data, res, 0);
			TEST_IPC_CHECK_EQUAL(data, bufsz, 1000);

			memset(buf, 0, sizeof(buf));
			res = (int)read(fds[0], buf, sizeof(buf) - 1);
			TEST_IPC_CHECK_EQUAL(data, res, 5);
			TEST_IPC_CHECK_STRING_EQUAL(data, buf, "pipe3");

			memset(buf, 0, sizeof(buf));
			res = (int)read(fds[1], buf, sizeof(buf) - 1);
			TEST_IPC_CHECK_EQUAL(data, res, 5);
			TEST_IPC_CHECK_STRING_EQUAL(data, buf, "pipe4");

			data->flags |= 1 << pomp_msg_get_id(msg);
		} else if (pomp_msg_get_id(msg) == 3) {
			res = pomp_msg_read(msg, "%x%p%u%x",
					&fds[0],
					&pbuf, &bufsz,
					&fds[1]);
			TEST_IPC_CHECK_EQUAL(data, res, 0);
			TEST_IPC_CHECK_EQUAL(data, bufsz, 1000);

			memset(buf, 0, sizeof(buf));
			res = (int)read(fds[0], buf, sizeof(buf) - 1);
			TEST_IPC_CHECK_EQUAL(data, res, 5);
			TEST_IPC_CHECK_STRING_EQUAL(data, buf, "pipe5");

			memset(buf, 0, sizeof(buf));
			res = (int)read(fds[1], buf, sizeof(buf) - 1);
			TEST_IPC_CHECK_EQUAL(data, res, 5);
			TEST_IPC_CHECK_STRING_EQUAL(data, buf, "pipe6");

			data->flags |= 1 << pomp_msg_get_id(msg);
		} else if (pomp_msg_get_id(msg) == 4) {
			res = pomp_msg_read(msg, "%p%u", &pbuf, &bufsz);
			TEST_IPC_CHECK_EQUAL(data, res, 0);
			TEST_IPC_CHECK_EQUAL(data, bufsz, 2000);

			data->flags |= 1 << pomp_msg_get_id(msg);
		} else if (pomp_msg_get_id(msg) == 5) {
			res = pomp_msg_read(msg, "%x%p%u%x%x",
					&fds[0],
					&pbuf, &bufsz,
					&fds[1],
					&fds[2]);
			TEST_IPC_CHECK_EQUAL(data, res, 0);
			TEST_IPC_CHECK_EQUAL(data, bufsz, 1000);

			memset(buf, 0, sizeof(buf));
			res = (int)read(fds[0], buf, sizeof(buf) - 1);
			TEST_IPC_CHECK_EQUAL(data, res, 5);
			TEST_IPC_CHECK_STRING_EQUAL(data, buf, "pipe7");

			memset(buf, 0, sizeof(buf));
			res = (int)read(fds[1], buf, sizeof(buf) - 1);
			TEST_IPC_CHECK_EQUAL(data, res, 5);
			TEST_IPC_CHECK_STRING_EQUAL(data, buf, "pipe8");

			memset(buf, 0, sizeof(buf));
			res = (int)read(fds[2], buf, sizeof(buf) - 1);
			TEST_IPC_CHECK_EQUAL(data, res, 5);
			TEST_IPC_CHECK_STRING_EQUAL(data, buf, "pipe9");

			data->flags |= 1 << pomp_msg_get_id(msg);
		}

		if (data->flags == 0x3f)
			pomp_conn_disconnect(conn);

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
static CU_TestInfo s_ipc_tests[] = {
	{(char *)"fd_passing", &test_fd_passing},
	CU_TEST_INFO_NULL,
};

/** */
/*extern*/ CU_SuiteInfo g_suites_ipc[] = {
	{(char *)"ipc", NULL, NULL, s_ipc_tests},
	CU_SUITE_INFO_NULL,
};

#endif /* !_WIN32 */
