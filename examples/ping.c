/**
 * @file ping.c
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

#include "ping_common.h"

#define MSG_PING	1
#define MSG_PONG	2

#define DIAG_PFX "PING: "

#define diag(_fmt, ...) \
	fprintf(stderr, DIAG_PFX _fmt "\n", ##__VA_ARGS__)

#define diag_errno(_func) \
	diag("%s error=%d(%s)", _func, errno, strerror(errno))

#define diag_fd_errno(_func, _fd) \
	diag("%s (fd=%d) : err=%d(%s)", _func, _fd, errno, strerror(errno))

/** */
struct app {
	int stop;
	struct pomp_timer  *timer;
	struct pomp_loop   *loop;
	struct pomp_ctx    *ctx;
};
static struct app s_app = {0, NULL, NULL, NULL};

/**
 */
static void log_conn_event(struct pomp_conn *conn, int is_server)
{
	const struct sockaddr *local_addr = NULL;
	const struct sockaddr *peer_addr = NULL;
	uint32_t local_addrlen = 0;
	uint32_t peer_addrlen = 0;

	/* Get local/peer addresses */
	local_addr = pomp_conn_get_local_addr(conn, &local_addrlen);
	peer_addr = pomp_conn_get_peer_addr(conn, &peer_addrlen);

	if (local_addr == NULL || local_addrlen == 0) {
		diag("Invalid local address");
		return;
	}
	if (peer_addr == NULL || peer_addrlen == 0) {
		diag("Invalid peer address");
		return;
	}

	if (pomp_addr_is_unix(local_addr, local_addrlen)) {
		char addrbuf[128] = "";
		const struct pomp_cred *peer_cred = NULL;

		/* Format using either local or peer address depending on
		 * client/server side */
		if (is_server) {
			pomp_addr_format(addrbuf, sizeof(addrbuf),
					local_addr, local_addrlen);
		} else {
			pomp_addr_format(addrbuf, sizeof(addrbuf),
					peer_addr, peer_addrlen);
		}

		/* Get peer credentials */
		peer_cred = pomp_conn_get_peer_cred(conn);
		if (peer_cred == NULL) {
			diag("%s pid=%d,uid=%u,gid=%u -> unknown",
					addrbuf, getpid(), getuid(), getgid());
		} else {
			diag("%s pid=%d,uid=%u,gid=%u -> pid=%d,uid=%u,gid=%u",
					addrbuf, getpid(), getuid(), getgid(),
					peer_cred->pid,
					peer_cred->uid,
					peer_cred->gid);
		}

	} else {
		char local_addrbuf[128] = "";
		char peer_addrbuf[128] = "";

		/* Format both addresses and log connection */
		pomp_addr_format(local_addrbuf, sizeof(local_addrbuf),
				local_addr, local_addrlen);
		pomp_addr_format(peer_addrbuf, sizeof(peer_addrbuf),
				peer_addr, peer_addrlen);
		diag("%s -> %s", local_addrbuf, peer_addrbuf);
	}
}

/**
 */
static void dump_msg(const struct pomp_msg *msg)
{
	uint32_t msgid = 0;
	uint32_t count = 0;
	char *str = NULL;

	msgid = pomp_msg_get_id(msg);
	switch (msgid) {
	case MSG_PING:
		pomp_msg_read(msg, "%u%ms", &count, &str);
		diag("MSG_PING  : %u %s", count, str);
		free(str);
		break;

	case MSG_PONG:
		pomp_msg_read(msg, "%u%ms", &count, &str);
		diag("MSG_PONG  : %u %s", count, str);
		free(str);
		break;

	default:
		diag("MSG_UNKNOWN : %u", msgid);
		break;
	}
}

/**
 */
static void server_event_cb(struct pomp_ctx *ctx, enum pomp_event event,
		struct pomp_conn *conn, const struct pomp_msg *msg,
		void *userdata)
{
	uint32_t count = 0;
	char *str = NULL;
	diag("%s : event=%d(%s) conn=%p msg=%p", __func__,
			event, pomp_event_str(event), conn, msg);

	switch (event) {
	case POMP_EVENT_CONNECTED:
	case POMP_EVENT_DISCONNECTED:
		log_conn_event(conn, 1);
		break;

	case POMP_EVENT_MSG:
		dump_msg(msg);
		if (pomp_msg_get_id(msg) == MSG_PING) {
			pomp_msg_read(msg, "%u%ms", &count, &str);
			pomp_conn_send(conn, MSG_PONG, "%u%s", count, "PONG");
			free(str);
		}
		break;

	default:
		diag("Unknown event : %d", event);
		break;
	}
}

/**
 */
static int server_start(const struct sockaddr *addr, uint32_t addrlen)
{
	int res = 0;

	/* Start listening for incoming connections */
	res = pomp_ctx_listen(s_app.ctx, addr, addrlen);
	if (res < 0) {
		diag("pomp_ctx_listen : err=%d(%s)", res, strerror(-res));
		goto out;
	}

out:
	return res;
}

/**
 */
static void client_event_cb(struct pomp_ctx *ctx, enum pomp_event event,
		struct pomp_conn *conn, const struct pomp_msg *msg,
		void *userdata)
{
	diag("%s : event=%d(%s) conn=%p msg=%p", __func__,
			event, pomp_event_str(event), conn, msg);

	switch (event) {
	case POMP_EVENT_CONNECTED:
	case POMP_EVENT_DISCONNECTED:
		log_conn_event(conn, 0);
		break;

	case POMP_EVENT_MSG:
		dump_msg(msg);
		break;

	default:
		diag("Unknown event : %d", event);
		break;
	}
}

/**
 */
static void client_timer(struct pomp_timer *timer, void *userdata)
{
	static int count;
	/* Send a message, re-launch timer */
	pomp_ctx_send(s_app.ctx, MSG_PING, "%u%s", ++count, "PING");
	pomp_timer_set(s_app.timer, 2000);
}

/**
 */
static int client_start(const struct sockaddr *addr, uint32_t addrlen)
{
	int res = 0;

	/* Create timer */
	s_app.timer = pomp_timer_new(s_app.loop, client_timer, NULL);

	/* Setup timer to raise in 2sec */
	pomp_timer_set(s_app.timer, 2000);

	/* Start connection to server */
	res = pomp_ctx_connect(s_app.ctx, addr, addrlen);
	if (res < 0) {
		diag("pomp_ctx_connect : err=%d(%s)", res, strerror(-res));
		goto out;
	}

out:
	return res;
}

/**
 */
static void sig_handler(int signum)
{
	diag("signal %d(%s) received", signum, strsignal(signum));
	s_app.stop = 1;
	if (s_app.loop != NULL)
		pomp_loop_wakeup(s_app.loop);
}

/**
 */
static void usage(const char *progname)
{
	fprintf(stderr, "%s -s <addr>\n", progname);
	fprintf(stderr, "    start server\n");
	fprintf(stderr, "%s -c <addr>\n", progname);
	fprintf(stderr, "    start client\n");
	fprintf(stderr, "<addr> format:\n");
	fprintf(stderr, "  inet:<addr>:<port>\n");
	fprintf(stderr, "  inet6:<addr>:<port>\n");
	fprintf(stderr, "  unix:<path>\n");
	fprintf(stderr, "  unix:@<name>\n");
}

/**
 */
int main(int argc, char *argv[])
{
	struct sockaddr_storage addr_storage;
	struct sockaddr *addr = NULL;
	uint32_t addrlen = 0;
	int is_server = 0;

	/* Check arguments */
	if (argc != 3 || (strcmp(argv[1], "-s") != 0
			&& strcmp(argv[1], "-c") != 0)) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	/* Client/Server mode */
	is_server = (strcmp(argv[1], "-s") == 0);

	/* Create context BEFORE parsing address
	 * (required for WIN32 as it is the lib that initialize winsock API) */
	s_app.ctx = pomp_ctx_new(is_server ? &server_event_cb :
			&client_event_cb, NULL);
	s_app.loop = pomp_ctx_get_loop(s_app.ctx);

	/* Parse address */
	memset(&addr_storage, 0, sizeof(addr_storage));
	addr = (struct sockaddr *)&addr_storage;
	addrlen = sizeof(addr_storage);
	if (pomp_addr_parse(argv[2], addr, &addrlen) < 0) {
		diag("Failed to parse address : %s", argv[2]);
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	/* Attach sig handler */
	signal(SIGINT, &sig_handler);
	signal(SIGTERM, &sig_handler);

	if (is_server) {
		if (server_start(addr, addrlen) < 0)
			goto out;
	} else {
		/* Client mode */
		if (client_start(addr, addrlen) < 0)
			goto out;
	}

	/* Run main loop until signal handler ask for stop */
	while (!s_app.stop)
		pomp_loop_wait_and_process(s_app.loop, -1);

out:
	/* Cleanup */
	if (s_app.timer != NULL) {
		pomp_timer_destroy(s_app.timer);
		s_app.timer = NULL;
	}
	if (s_app.ctx != NULL) {
		pomp_ctx_stop(s_app.ctx);
		pomp_ctx_destroy(s_app.ctx);
		s_app.ctx = NULL;
		s_app.loop = NULL;
	}

	return 0;
}
