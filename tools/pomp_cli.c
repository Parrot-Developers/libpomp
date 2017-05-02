/**
 * @file pomp_cli.c
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

/* Win32 headers */
#ifdef _WIN32
#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT 0x0501
#  endif /* !_WIN32_WINNT */
#  include <winsock2.h>
#endif /* _WIN32 */

/* Standard headers */
#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif /* !_GNU_SOURCE */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

/* Unix headers */
#ifndef _WIN32
#  include <unistd.h>
#  include <sys/socket.h>
#endif /* !_WIN32 */

#include "libpomp.h"

#define DIAG_PFX "POMPCLI: "

#define diag(_fmt, ...) \
	fprintf(stderr, DIAG_PFX _fmt "\n", ##__VA_ARGS__)

#define diag_errno(_func) \
	diag("%s error=%d(%s)", _func, errno, strerror(errno))

#define diag_fd_errno(_func, _fd) \
	diag("%s (fd=%d) : err=%d(%s)", _func, _fd, errno, strerror(errno))

/* Win32 stubs */
#ifdef _WIN32
static inline const char *strsignal(int signum) { return "??"; }
#endif /* _WIN32 */

/** */
struct app {
	int                     timeout;
	int                     dump;
	struct sockaddr         *addr;
	uint32_t                addrlen;
	struct sockaddr         *addrto;
	uint32_t                addrtolen;
	int                     hasmsg;
	uint32_t                msgid;
	const char              *msgfmt;
	int                     msgargc;
	const char * const      *msgargv;
	int                     running;
	struct pomp_loop        *loop;
	struct pomp_timer       *timer;
	struct pomp_ctx         *ctx;
	int                     waitmsg;
	uint32_t                expected_msgid;
};
static struct app s_app = {
		.timeout = -1,
		.dump = 0,
		.addr = NULL,
		.addrlen = 0,
		.addrto = NULL,
		.addrtolen = 0,
		.hasmsg = 0,
		.msgid = 0,
		.msgfmt = NULL,
		.msgargc = -1,
		.msgargv = NULL,
		.running = 0,
		.loop = NULL,
		.timer = NULL,
		.ctx = NULL,
		.waitmsg = 0,
		.expected_msgid = 0,
};

/**
 *
 */
static void timer_cb(struct pomp_timer *timer, void *userdata)
{
	diag("Timeout !");
	s_app.running = 0;
}

/**
 *
 */
static int setup_timeout()
{
	int res = 0;

	/* Create timer */
	if (s_app.timer == NULL) {
		s_app.timer = pomp_timer_new(s_app.loop, &timer_cb, NULL);
		if (s_app.timer == NULL)
			goto error;
	}

	/* Set timer */
	res = pomp_timer_set(s_app.timer, s_app.timeout * 1000);
	if (res < 0) {
		diag("pomp_timer_set: err=%d(%s)", res, strerror(-res));
		goto error;
	}

	return 0;

	/* Cleanup in case of error */
error:
	if (s_app.timer != NULL) {
		pomp_timer_destroy(s_app.timer);
		s_app.timer = NULL;
	}
	return res;
}

/**
 *
 */
static void cancel_timeout()
{
	if (s_app.timer != NULL) {
		pomp_timer_clear(s_app.timer);
		pomp_timer_destroy(s_app.timer);
		s_app.timer = NULL;
	}
}

/**
 *
 */
static int send_msg()
{
	int res = 0;
	struct pomp_msg *msg = NULL;

	/* Create message */
	msg = pomp_msg_new();
	if (msg == NULL) {
		res = -ENOMEM;
		goto error;
	}

	/* Encode message */
	res = pomp_msg_write_argv(msg, s_app.msgid, s_app.msgfmt,
			s_app.msgargc, s_app.msgargv);
	if (res < 0) {
		diag("pomp_msg_write_argv: err=%d(%s)", res, strerror(-res));
		goto error;
	}

	/* Send it */
	if (s_app.addrto != NULL) {
		res = pomp_ctx_send_msg_to(s_app.ctx, msg,
				s_app.addrto, s_app.addrtolen);
	} else {
		res = pomp_ctx_send_msg(s_app.ctx, msg);
	}

	if (res < 0) {
		diag("pomp_ctx_send_msg: err=%d(%s)", res, strerror(-res));
		goto error;
	}

	/* Cleanup */
error:
	if (msg != NULL)
		pomp_msg_destroy(msg);
	return res;
}

/**
 *
 */
static void event_cb(struct pomp_ctx *ctx, enum pomp_event event,
		struct pomp_conn *conn, const struct pomp_msg *msg,
		void *userdata)
{
	int res;
	char *buf = NULL;

	diag("%s : event=%d(%s) conn=%p msg=%p", __func__,
			event, pomp_event_str(event), conn, msg);

	switch (event) {
	case POMP_EVENT_CONNECTED:
		/* Send message once connected (ignore errors) */
		if (s_app.hasmsg) {
			send_msg();
			s_app.hasmsg = 0;
		}

		/* Exit loop if not dumping message */
		if (!s_app.dump && !s_app.waitmsg)
			s_app.running = 0;
		else if (!s_app.waitmsg)
			cancel_timeout();
		break;

	case POMP_EVENT_DISCONNECTED:
		/* Setup timeout if needed (ignore errors) */
		if (s_app.timeout >= 0)
			setup_timeout();
		break;

	case POMP_EVENT_MSG:
		if (s_app.dump) {
			res = pomp_msg_adump(msg, &buf);
			if (res < 0) {
				diag("pomp_msg_adump: err=%d(%s)", res,
						strerror(-res));
				return;
			}
			diag("MSG: %s", buf);
			free(buf);
		}
		if (s_app.waitmsg &&
				pomp_msg_get_id(msg) == s_app.expected_msgid) {
			s_app.running = 0;
		}
		break;

	default:
		diag("Unknown event : %d", event);
		break;
	}
}

/**
 *
 */
static void sig_handler(int signum)
{
	diag("signal %d(%s) received", signum, strsignal(signum));
	s_app.running = 0;
	if (s_app.loop != NULL)
		pomp_loop_wakeup(s_app.loop);
}

/**
 */
static void usage(const char *progname)
{
	fprintf(stderr, "usage: %s [<options>] <addr> [[<addrto>] <msgid>"
			" [<fmt> [<args>...]]]\n",
			progname);
	fprintf(stderr, "Send a pomp message on a socket or dump messages\n");
	fprintf(stderr, "received on a socket\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  <options>: see below\n");
	fprintf(stderr, "  <addr>  : address\n");
	fprintf(stderr, "  <addrto>: address to send message to for udp\n");
	fprintf(stderr, "  <msgid> : message id\n");
	fprintf(stderr, "  <fmt>   : message format\n");
	fprintf(stderr, "  <args>  : message arguments\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "<addr> format:\n");
	fprintf(stderr, "  inet:<addr>:<port>\n");
	fprintf(stderr, "  inet6:<addr>:<port>\n");
	fprintf(stderr, "  unix:<path>\n");
	fprintf(stderr, "  unix:@<name>\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  -h --help   : print this help message and exit\n");
	fprintf(stderr, "  -s --server : use a server socket\n");
	fprintf(stderr, "  -c --client : use a client socket (default)\n");
	fprintf(stderr, "  -u --udp    : use a udp socket\n");
	fprintf(stderr, "  -d --dump   : stay connected and dump messages\n");
	fprintf(stderr, "  -w --wait   : wait until a message is received\n");
	fprintf(stderr, "                with the given message id\n");
	fprintf(stderr, "  -t --timeout: timeout to wait connection\n");
	fprintf(stderr, "                in seconds (default no timeout)\n");
	fprintf(stderr, "\n");
}

/**
 */
int main(int argc, char *argv[])
{
	int res = 0;
	int server = 0;
	int udp = 0;
	int argidx = 0;
	const char *arg_addr = NULL;
	const char *arg_addrto = NULL;
	const char *arg_msgid = NULL;
	struct sockaddr_storage addr_storage;
	struct sockaddr_storage addrto_storage;

	/* Parse options */
	for (argidx = 1; argidx < argc; argidx++) {
		if (argv[argidx][0] != '-') {
			/* End of options */
			break;
		} else if (strcmp(argv[argidx], "-h") == 0
				|| strcmp(argv[argidx], "--help") == 0) {
			/* Help */
			usage(argv[0]);
			goto out;
		} else if (strcmp(argv[argidx], "-s") == 0
				|| strcmp(argv[argidx], "--server") == 0) {
			server = 1;
		} else if (strcmp(argv[argidx], "-c") == 0
				|| strcmp(argv[argidx], "--client") == 0) {
			server = 0;
		} else if (strcmp(argv[argidx], "-u") == 0
				|| strcmp(argv[argidx], "--udp") == 0) {
			udp = 1;
		} else if (strcmp(argv[argidx], "-d") == 0
				|| strcmp(argv[argidx], "--dump") == 0) {
			s_app.dump = 1;
		} else if (strcmp(argv[argidx], "-t") == 0
				|| strcmp(argv[argidx], "--timeout") == 0) {
			if (++argidx >= argc) {
				diag("Missing timeout value");
				goto error;
			}
			s_app.timeout = strtol(argv[argidx], NULL, 10);
		} else if (strcmp(argv[argidx], "-w") == 0
				|| strcmp(argv[argidx], "--wait") == 0) {
			if (++argidx >= argc) {
				diag("Missing expected message id");
				goto error;
			}
			s_app.expected_msgid = strtol(argv[argidx], NULL, 10);
			s_app.waitmsg = 1;
		} else {
			diag("Unknown option: '%s'", argv[argidx]);
			goto error;
		}
	}

	/* Create pomp context, get loop BEFORE parsing address
	 * (required for WIN32 as it is the lib that initialize winsock API) */
	s_app.ctx = pomp_ctx_new(&event_cb, NULL);
	if (s_app.ctx == NULL)
		goto error;
	s_app.loop = pomp_ctx_get_loop(s_app.ctx);

	/* Get address */
	if (argc - argidx >= 1) {
		arg_addr = argv[argidx++];

		/* Parse address */
		memset(&addr_storage, 0, sizeof(addr_storage));
		s_app.addr = (struct sockaddr *)&addr_storage;
		s_app.addrlen = sizeof(addr_storage);
		if (pomp_addr_parse(arg_addr, s_app.addr, &s_app.addrlen) < 0) {
			diag("Failed to parse address : %s", arg_addr);
			goto error;
		}
	} else {
		diag("Missing address");
		goto error;
	}

	/* Get destination address for udp (optional if dumping) */
	if (udp) {
		if (argc - argidx >= 1) {
			arg_addrto = argv[argidx++];

			/* Parse address */
			memset(&addrto_storage, 0, sizeof(addrto_storage));
			s_app.addrto = (struct sockaddr *)&addrto_storage;
			s_app.addrtolen = sizeof(addrto_storage);
			if (pomp_addr_parse(arg_addrto, s_app.addrto,
					&s_app.addrtolen) < 0) {
				diag("Failed to parse address: %s", arg_addrto);
				goto error;
			}
		} else if (!s_app.dump) {
			diag("Missing destination address");
			goto error;
		}
	}

	/* Get message id (optional if dumping) */
	if (argc - argidx >= 1) {
		arg_msgid = argv[argidx++];
		s_app.msgid = strtoul(arg_msgid, NULL, 10);
		s_app.hasmsg = 1;
	} else if (!s_app.dump) {
		diag("Missing message id");
		goto error;
	}

	/* Get message format (optional) */
	if (argc - argidx >= 1)
		s_app.msgfmt = argv[argidx++];

	/* Get message arguments (optional) */
	if (argc - argidx >= 1) {
		s_app.msgargc = argc - argidx;
		s_app.msgargv = (const char * const *)&argv[argidx];
		argidx += s_app.msgargc;
	}

	/* Attach sig handler */
	s_app.running = 1;
	signal(SIGINT, &sig_handler);
	signal(SIGTERM, &sig_handler);

	/* Setup timeout if needed (ignore errors) */
	if (s_app.timeout >= 0)
		setup_timeout();

	/* Connect or listen or bind */
	if (udp)
		res = pomp_ctx_bind(s_app.ctx, s_app.addr, s_app.addrlen);
	else if (server)
		res = pomp_ctx_listen(s_app.ctx, s_app.addr, s_app.addrlen);
	else
		res = pomp_ctx_connect(s_app.ctx, s_app.addr, s_app.addrlen);
	if (res < 0) {
		diag("pomp_ctx_%s : err=%d(%s)", udp ? "bind" :
				server ? "listen" : "connect",
				res, strerror(-res));
		goto error;
	}

	if (udp) {
		/* Send message now for udp */
		if (s_app.hasmsg)
			send_msg();

		/* Do not run loop if not dumping or waiting */
		if (!s_app.dump && !s_app.waitmsg)
			goto out;
	}

	/* Run loop */
	while (s_app.running)
		pomp_loop_wait_and_process(s_app.loop, -1);

	goto out;

error:
	res = -1;
out:
	/* Cleanup */
	if (s_app.ctx != NULL) {
		pomp_ctx_stop(s_app.ctx);
		cancel_timeout();
		pomp_ctx_destroy(s_app.ctx);
		s_app.ctx = NULL;
		s_app.loop = NULL;
	}
	return res == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
