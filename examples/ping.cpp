/**
 * @file ping.cpp
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
#include "libpomp.hpp"

#ifndef POMP_CXX11
#  error "This code requires c++11 features"
#endif /* !POMP_CXX11 */

#define DIAG_PFX "PING: "

#define diag(_fmt, ...) \
	fprintf(stderr, DIAG_PFX _fmt "\n", ##__VA_ARGS__)

#define diag_errno(_func) \
	diag("%s error=%d(%s)", _func, errno, strerror(errno))

#define diag_fd_errno(_func, _fd) \
	diag("%s (fd=%d) : err=%d(%s)", _func, _fd, errno, strerror(errno))

/**
 */
static const uint32_t MSG_PING = 1;
static const uint32_t MSG_PONG = 2;

typedef pomp::MessageFormat<MSG_PING, pomp::ArgU32, pomp::ArgStr> MsgFmtPing;
typedef pomp::MessageFormat<MSG_PONG, pomp::ArgU32, pomp::ArgStr> MsgFmtPong;

static void log_conn_event(pomp::Connection *conn, bool is_server);
static void dump_msg(const pomp::Message &msg);
static void usage(const char *progname);

namespace {

/**
 */
class PompHandler : public pomp::EventHandler {
protected:
	bool           mIsServer;
	pomp::Context  *mCtx;

public:
	inline PompHandler(bool isServer) {
		mIsServer = isServer;
		mCtx = new pomp::Context(this);
	}

	inline virtual ~PompHandler() {
		delete mCtx;
	}

	inline virtual void onConnected(pomp::Context *ctx, pomp::Connection *conn) {
		diag("CONNECTED");
		log_conn_event(conn, mIsServer);
	}

	inline virtual void onDisconnected(pomp::Context *ctx, pomp::Connection *conn) {
		diag("DISCONNECTED");
		log_conn_event(conn, mIsServer);
	}

	inline pomp::Loop *getLoop() const {
		return mCtx->getLoop();
	}

	virtual int start(const struct sockaddr *addr, uint32_t addrlen) = 0;
	virtual int stop() = 0;
};

/**
 */
class Server : public PompHandler {
public:
	inline Server() : PompHandler(true) {}
	inline virtual ~Server() {}

	inline virtual int start(const struct sockaddr *addr, uint32_t addrlen) {
		mCtx->listen(addr, addrlen);
		return 0;
	}

	inline virtual int stop() {
		mCtx->stop();
		return 0;
	}

	inline virtual void recvMessage(pomp::Context *ctx, pomp::Connection *conn, const pomp::Message &msg) {
		diag("Server: MESSAGE");
		dump_msg(msg);
		if (msg.getId() == MSG_PING) {
			uint32_t count = 0;
			std::string str;
			msg.read<MsgFmtPing>(count, str);
			conn->send<MsgFmtPong>(count, "PONG");
		}
	}
};

/**
 */
class Client : public PompHandler {
private:
	uint32_t                 mCount;
	pomp::Timer              *mTimer;
	pomp::Timer::HandlerFunc mTimerHandler;

private:
	inline void processTimer() {
		/* Send a message, re-launch timer */
		mCtx->send<MsgFmtPing>(++mCount, "PING");
		mTimer->set(2000);
	}

public:
	inline Client() : PompHandler(false) {
		mTimerHandler.set(std::bind(&Client::processTimer, this));
		mTimer = new pomp::Timer(mCtx->getLoop(), &mTimerHandler);
		mCount = 0;
	}

	inline virtual ~Client() {
		delete mTimer;
	}

	inline virtual int start(const struct sockaddr *addr, uint32_t addrlen) {
		mCtx->connect(addr, addrlen);

		/* Set timer */
		mTimer->set(2000);
		return 0;
	}

	inline virtual int stop() {
		mCtx->stop();
		return 0;
	}

	inline virtual void recvMessage(pomp::Context *ctx, pomp::Connection *conn, const pomp::Message &msg) {
		diag("Client: MESSAGE");
		dump_msg(msg);
	}
};

/**
 */
class App {
private:
	static App   *sInstance;
	PompHandler  *mPompHandler;
	bool         mRunning;

private:
	inline static void sigHandler(int signum) {
		diag("signal %d(%s) received", signum, strsignal(signum));
		App::sInstance->mRunning = false;
		App::sInstance->mPompHandler->getLoop()->wakeup();
	}

public:
	inline App(bool isServer) {
		/* Save single instance */
		App::sInstance = this;

		/* Initialize parameters */
		if (isServer)
			mPompHandler = new Server();
		else
			mPompHandler = new Client();
		mRunning = false;
	}

	inline ~App() {
		/* Free resources*/
		delete mPompHandler;

		/* Reset single instance */
		App::sInstance = NULL;
	}

	inline int run(const struct sockaddr *addr, uint32_t addrlen) {
		/* Attach sig handler */
		mRunning = true;
		signal(SIGINT, &App::sigHandler);
		signal(SIGTERM, &App::sigHandler);

		/* Start running */
		mPompHandler->start(addr, addrlen);

		/* Run loop */
		while (mRunning)
			mPompHandler->getLoop()->waitAndProcess(-1);

		/* Stop everything */
		mPompHandler->stop();

		/* Detach sig handler */
		signal(SIGINT, SIG_DFL);
		signal(SIGTERM, SIG_DFL);
		return 0;
	}
};
App *App::sInstance;

} /* anonymous namespace */

/**
 */
static void log_conn_event(pomp::Connection *conn, bool is_server)
{
	const struct sockaddr *local_addr = NULL;
	const struct sockaddr *peer_addr = NULL;
	uint32_t local_addrlen = 0;
	uint32_t peer_addrlen = 0;

	/* Get local/peer addresses */
	local_addr = conn->getLocalAddr(&local_addrlen);
	peer_addr = conn->getPeerAddr(&peer_addrlen);

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
		peer_cred = conn->getPeerCred();
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
static void dump_msg(const pomp::Message &msg)
{
	uint32_t msgid = 0;
	uint32_t count = 0;
	std::string str;

	msgid = msg.getId();
	switch (msgid) {
	case MSG_PING:
		msg.read<MsgFmtPing>(count, str);
		diag("MSG_PING  : %u %s", count, str.c_str());
		break;

	case MSG_PONG:
		msg.read<MsgFmtPong>(count, str);
		diag("MSG_PONG  : %u %s", count, str.c_str());
		break;

	default:
		diag("MSG_UNKNOWN : %u", msgid);
		break;
	}
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

	/* Check arguments */
	if (argc != 3 || (strcmp(argv[1], "-s") != 0
			&& strcmp(argv[1], "-c") != 0)) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	/* Client/Server mode */
	bool isServer = (strcmp(argv[1], "-s") == 0);

	/* Create application BEFORE parsing address
	 * (required for WIN32 as it is the lib that initialize winsock API) */
	App app(isServer);

	/* Parse address */
	memset(&addr_storage, 0, sizeof(addr_storage));
	addr = reinterpret_cast<struct sockaddr *>(&addr_storage);
	addrlen = sizeof(addr_storage);
	if (pomp_addr_parse(argv[2], addr, &addrlen) < 0) {
		diag("Failed to parse address : %s", argv[2]);
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	/* Run application */
	return app.run(addr, addrlen);
}
