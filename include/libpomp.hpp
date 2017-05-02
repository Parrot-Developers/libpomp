/**
 * @file libpomp.hpp
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

#ifndef _LIBPOMP_HPP_
#define _LIBPOMP_HPP_

#include <errno.h>

#include <string>
#include <utility>
#include <vector>
#include <functional>

/* Detect support for C++11 */
#if defined(__cplusplus) && (__cplusplus >= 201103L)
#  define POMP_CXX11
#elif defined(__GNUC__) && defined(__GXX_EXPERIMENTAL_CXX0X__)
#  define POMP_CXX11
#endif

/** Disable copy constructor and assignment operator */
#define POMP_DISABLE_COPY(_cls) \
	private: \
		_cls(const _cls &); \
		_cls &operator=(const _cls &);

#include "libpomp.h"
#ifdef POMP_CXX11
#  include "libpomp-cxx11.hpp"
#endif /* POMP_CXX11 */

namespace pomp {

/* Forward declarations */
class Message;
class Connection;
class EventHandler;
class Loop;
class Timer;
class Context;

/**
 * Message class.
 */
class Message {
	POMP_DISABLE_COPY(Message)
private:
	struct pomp_msg        *mMsg;       /**< Internal message */
	const struct pomp_msg  *mConstMsg;  /**< Internal const message */
	friend class Connection;
	friend class Context;

private:
	/** Internal constructor from a const message. */
	inline Message(const struct pomp_msg *msg) {
		mMsg = NULL;
		mConstMsg = msg;
	}

	/** Get internal message. */
	inline const struct pomp_msg *getMsg() const {
		return mMsg != NULL ? mMsg : mConstMsg;
	}

public:
	/** Constructor. */
	inline Message() {
		mMsg = pomp_msg_new();
		mConstMsg = NULL;
	}

	/** Destructor. */
	inline ~Message() {
		if (mMsg != NULL)
			pomp_msg_destroy(mMsg);
	}

	/** Get message Id. */
	inline uint32_t getId() const {
		return pomp_msg_get_id(getMsg());
	}

	/** Write and encode a message. */
	inline int write(uint32_t msgid, const char *fmt, ...) POMP_ATTRIBUTE_FORMAT_PRINTF(3, 4) {
		va_list args;
		va_start(args, fmt);
		pomp_msg_clear(mMsg);
		int res = pomp_msg_writev(mMsg, msgid, fmt, args);
		va_end(args);
		return res;
	}

	/** Write and encode a message. */
	inline int writev(uint32_t msgid, const char *fmt, va_list args) {
		pomp_msg_clear(mMsg);
		return pomp_msg_writev(mMsg, msgid, fmt, args);
	}

	/** Read and decode a message. */
	inline int read(const char *fmt, ...) const POMP_ATTRIBUTE_FORMAT_SCANF(2, 3) {
		va_list args;
		va_start(args, fmt);
		int res = pomp_msg_readv(getMsg(), fmt, args);
		va_end(args);
		return res;
	}

	/** Read and decode a message. */
	inline int readv(const char *fmt, va_list args) const {
		return pomp_msg_readv(getMsg(), fmt, args);
	}

#ifdef POMP_CXX11
	/** Write and encode a message. */
	template<typename Fmt, typename... ArgsW>
	inline int write(const ArgsW&... args) {
		if (mMsg == NULL)
			return -EINVAL;
		struct pomp_encoder *enc = pomp_encoder_new();
		pomp_msg_clear(mMsg);
		pomp_msg_init(mMsg, Fmt::id);
		pomp_encoder_init(enc, mMsg);
		int res = Fmt::encode(enc, std::forward<const ArgsW&>(args)...);
		pomp_msg_finish(mMsg);
		pomp_encoder_destroy(enc);
		return res;
	}

	/** Read and decode a message. */
	template<typename Fmt, typename... ArgsR>
	inline int read(ArgsR&... args) const {
		if (getId() != Fmt::id)
			return -EINVAL;
		struct pomp_decoder *dec = pomp_decoder_new();
		pomp_decoder_init(dec, getMsg());
		int res = Fmt::decode(dec, std::forward<ArgsR&>(args)...);
		pomp_decoder_destroy(dec);
		return res;
	}
#endif /* POMP_CXX11 */
};

/**
 * Connection class.
 */
class Connection {
	POMP_DISABLE_COPY(Connection)
private:
	struct pomp_conn  *mConn;  /**< Internal connection */
	friend class Context;

private:
	/** Internal constructor. */
	inline Connection(struct pomp_conn *conn) {
		mConn = conn;
	}

public:
	/** Force disconnection of an established connection. */
	inline int disconnect() {
		return pomp_conn_disconnect(mConn);
	}

	/** Get connection local address. */
	inline const struct sockaddr *getLocalAddr(uint32_t *addrlen) {
		return pomp_conn_get_local_addr(mConn, addrlen);
	}

	/** Get connection remote peer address. */
	inline const struct sockaddr *getPeerAddr(uint32_t *addrlen) {
		return pomp_conn_get_peer_addr(mConn, addrlen);
	}

	/** Get connection remote peer credentials for local sockets. */
	inline const struct pomp_cred *getPeerCred() {
		return pomp_conn_get_peer_cred(mConn);
	}

	/** Get file descriptor associated with the connection. */
	inline int getFd() {
		return pomp_conn_get_fd(mConn);
	}

	/** Send a message to the peer of the connection. */
	inline int sendMsg(const Message &msg) {
		return pomp_conn_send_msg(mConn, msg.getMsg());
	}

	/** Format and send a message to the peer of the connection. */
	inline int send(uint32_t msgid, const char *fmt, ...) POMP_ATTRIBUTE_FORMAT_PRINTF(3, 4) {
		va_list args;
		va_start(args, fmt);
		int res = pomp_conn_sendv(mConn, msgid, fmt, args);
		va_end(args);
		return res;
	}

	/** Format and send a message to the peer of the connection. */
	inline int sendv(uint32_t msgid, const char *fmt, va_list args) {
		return pomp_conn_sendv(mConn, msgid, fmt, args);
	}

#ifdef POMP_CXX11
	/** Format and send a message to the peer of the connection. */
	template<typename Fmt, typename... ArgsW>
	inline int send(const ArgsW&... args) {
		Message msg;
		int res = msg.write<Fmt>(args...);
		if (res == 0)
			res = sendMsg(msg);
		return res;
	}
#endif /* POMP_CXX11 */
};

/** ConnectionArray class */
typedef std::vector<Connection *> ConnectionArray;

/**
 * Loop class.
 */
class Loop {
	POMP_DISABLE_COPY(Loop)
private:
	struct pomp_loop  *mLoop;  /**< Internal loop */
	bool              mOwner;  /**< True if owner of internal loop */
	friend class Context;
	friend class Timer;
private:
	/** Internal fd event callback */
	inline static void fdEventCb(int _fd, uint32_t _revents, void *_userdata) {
		Handler *handler = reinterpret_cast<Handler *>(_userdata);
		handler->processFd(_fd, _revents);
	}

	/** Internal constructor. */
	inline Loop(struct pomp_loop *loop) {
		mLoop = loop;
		mOwner = false;
	}

public:
	enum {
		EVENT_IN = POMP_FD_EVENT_IN,
		EVENT_PRI = POMP_FD_EVENT_PRI,
		EVENT_OUT = POMP_FD_EVENT_OUT,
		EVENT_ERR = POMP_FD_EVENT_ERR,
		EVENT_HUP = POMP_FD_EVENT_HUP,
	};

	/** Handler class */
	class Handler {
		POMP_DISABLE_COPY(Handler)
	public:
		inline Handler() {}
		inline virtual ~Handler() {}
		virtual void processFd(int fd, uint32_t revents) = 0;
	};

	/** Constructor. */
	inline Loop() {
		mLoop = pomp_loop_new();
		mOwner = true;
	}

	/** Destructor. */
	inline ~Loop() {
		if (mOwner)
			pomp_loop_destroy(mLoop);
	}

	/** Register a new fd in loop. */
	inline int add(int fd, uint32_t events, Handler *handler) {
		return pomp_loop_add(mLoop, fd, events, &Loop::fdEventCb, handler);
	}

	/** Modify the set of events to monitor for a registered fd. */
	inline int update(int fd, uint32_t events) {
		return pomp_loop_update(mLoop, fd, events);
	}

	/** Unregister a fd from the loop. */
	inline int remove(int fd) {
		return pomp_loop_remove(mLoop, fd);
	}

	/** Check if fd is registered */
	inline bool hasFd(int fd) {
		return pomp_loop_has_fd(mLoop, fd) != 0;
	}

	/** Get the fd/event of the loop. */
	inline intptr_t getFd() {
		return pomp_loop_get_fd(mLoop);
	}

	/** Function to be called when the loop is marked as ready. */
	inline int processFd() {
		return pomp_loop_process_fd(mLoop);
	}

	/** Wait for events to occur in loop and process them. */
	inline int waitAndProcess(int timeout) {
		return pomp_loop_wait_and_process(mLoop, timeout);
	}

	/** Wakeup the loop from another thread */
	inline int wakeup() {
		return pomp_loop_wakeup(mLoop);
	}

	/** Get internal pomp_loop */
	inline operator struct pomp_loop *() {
		return mLoop;
	}

#ifdef POMP_CXX11
	/** Handler wrapper that can take a std::function */
	class HandlerFunc : public Handler {
		POMP_DISABLE_COPY(HandlerFunc)
	public:
		typedef std::function<void (int fd, uint32_t revents)> Func;
		inline HandlerFunc() {}
		inline HandlerFunc(const Func &func) : mFunc(func) {}
		inline void set(const Func &func) {mFunc = func;}
		inline virtual void processFd(int fd, uint32_t revents) {mFunc(fd, revents);}
	private:
		Func mFunc;
	};
#endif /* POMP_CXX11 */
};

/**
 * Timer class.
 */
class Timer {
	POMP_DISABLE_COPY(Timer)
private:
	struct pomp_timer  *mTimer;  /**< Internal timer */
private:
	/** Internal timer callback */
	inline static void timerCb(struct pomp_timer *_timer, void *_userdata) {
		(void)_timer;
		Handler *handler = reinterpret_cast<Handler *>(_userdata);
		handler->processTimer();
	}

public:
	/** Handler class */
	class Handler {
		POMP_DISABLE_COPY(Handler)
	public:
		inline Handler() {}
		inline virtual ~Handler() {}
		virtual void processTimer() = 0;
	};

	/** Constructor. */
	inline Timer(Loop *loop, Handler *handler) {
		mTimer = pomp_timer_new(loop->mLoop, &Timer::timerCb, handler);
	}

	/** Destructor. */
	inline ~Timer() {
		pomp_timer_destroy(mTimer);
	}

	inline int set(uint32_t delay, uint32_t period = 0) {
		if (period == 0)
			return pomp_timer_set(mTimer, delay);
		else
			return pomp_timer_set_periodic(mTimer, delay, period);
	}

	inline int setPeriodic(uint32_t delay, uint32_t period) {
		return pomp_timer_set_periodic(mTimer, delay, period);
	}

	inline int clear() {
		return pomp_timer_clear(mTimer);
	}

#ifdef POMP_CXX11
	/** Handler wrapper that can take a std::function */
	class HandlerFunc : public Handler {
		POMP_DISABLE_COPY(HandlerFunc)
	public:
		typedef std::function<void ()> Func;
		inline HandlerFunc() {}
		inline HandlerFunc(const Func &func) : mFunc(func) {}
		inline void set(const Func &func) {mFunc = func;}
		inline virtual void processTimer() {mFunc();}
	private:
		Func mFunc;
	};
#endif /* POMP_CXX11 */
};

/**
 * EventHandler class.
 */
class EventHandler {
	POMP_DISABLE_COPY(EventHandler)
public:
	inline EventHandler() {}
	inline virtual ~EventHandler() {}
	inline virtual void onConnected(Context *ctx, Connection *conn) { (void)ctx; (void)conn; }
	inline virtual void onDisconnected(Context *ctx, Connection *conn) { (void)ctx; (void)conn; }
	inline virtual void recvMessage(Context *ctx, Connection *conn, const Message &msg) { (void)ctx; (void)conn; (void)msg; }
};

/**
 * Context class.
 */
class Context {
	POMP_DISABLE_COPY(Context)
private:
	struct pomp_ctx  *mCtx;           /**< Internal context */
	EventHandler     *mEventHandler;  /**< Event handler */
	ConnectionArray  mConnections;    /**< Connection array */
	Loop             *mLoop;          /**< Associated loop */
	bool             mExtLoop;        /**< True if loop is external */

private:
	/** Find our own connection object from internal one */
	inline ConnectionArray::iterator findConn(struct pomp_conn *_conn) {
		ConnectionArray::iterator it;
		for (it = mConnections.begin(); it != mConnections.end(); ++it) {
			if ((*it)->mConn == _conn)
				return it;
		}
		return mConnections.end();
	}

	/** Internal event callback */
	inline static void eventCb(struct pomp_ctx *_ctx,
			enum pomp_event _event,
			struct pomp_conn *_conn,
			const struct pomp_msg *_msg,
			void *_userdata) {
		(void)_ctx;

		/* Get our own object from user data */
		Context *self = reinterpret_cast<Context *>(_userdata);
		Connection *conn = NULL;
		ConnectionArray::iterator it;

		switch (_event) {
		case POMP_EVENT_CONNECTED:
			conn = new Connection(_conn);
			self->mConnections.push_back(conn);
			self->mEventHandler->onConnected(self, conn);
			break;

		case POMP_EVENT_DISCONNECTED:
			it = self->findConn(_conn);
			if (it != self->mConnections.end()) {
				conn = *it;
				self->mEventHandler->onDisconnected(self, conn);
				self->mConnections.erase(it);
				delete conn;
			}
			break;

		case POMP_EVENT_MSG:
			it = self->findConn(_conn);
			if (it != self->mConnections.end())
				conn = *it;
			self->mEventHandler->recvMessage(self, conn, Message(_msg));
			break;

		default:
			break;
		}
	}

public:
	/** Constructor. */
	inline Context(EventHandler *eventHandler, Loop *loop = NULL) {
		if (loop == NULL) {
			mCtx = pomp_ctx_new(&Context::eventCb, this);
			mEventHandler = eventHandler;
			mLoop = NULL;
			mExtLoop = false;
		} else {
			mCtx = pomp_ctx_new_with_loop(&Context::eventCb, this, loop->mLoop);
			mEventHandler = eventHandler;
			mLoop = loop;
			mExtLoop = true;
		}
	}

	/** Destructor */
	inline ~Context() {
		pomp_ctx_destroy(mCtx);
		if (mLoop != NULL && !mExtLoop)
			delete mLoop;
	}

	/** Start a server. */
	inline int listen(const struct sockaddr *addr, uint32_t addrlen) {
		return pomp_ctx_listen(mCtx, addr, addrlen);
	}

	/** Start a client. */
	inline int connect(const struct sockaddr *addr, uint32_t addrlen) {
		return pomp_ctx_connect(mCtx, addr, addrlen);
	}

	/** Bind a connection-less context (inet-udp). */
	inline int bind(const struct sockaddr *addr, uint32_t addrlen) {
		return pomp_ctx_bind(mCtx, addr, addrlen);
	}

	/** Stop the context. It will disconnects all peers (with notification). */
	inline int stop() {
		return pomp_ctx_stop(mCtx);
	}

	/** Get the fd/event of the loop associated with the context. */
	inline intptr_t getFd() {
		return pomp_ctx_get_fd(mCtx);
	}

	/** Function to be called when the loop of the context is marked as ready. */
	inline int processFd() {
		return pomp_ctx_process_fd(mCtx);
	}

	/** Wait for events to occur in context and process them. */
	inline int waitAndProcess(int timeout) {
		return pomp_ctx_wait_and_process(mCtx, timeout);
	}

	/** Wakeup the comtext from another thread */
	inline int wakeup() {
		return pomp_ctx_wakeup(mCtx);
	}

	/** Get the loop of the context. */
	inline Loop *getLoop() {
		if (mLoop == NULL)
			mLoop = new Loop(pomp_ctx_get_loop(mCtx));
		return mLoop;
	}

	/** Get first connection of context */
	inline Connection *getConnection() const {
		return mConnections.size() > 0 ? mConnections[0] : NULL;
	}

	/**  Get array of active connections. */
	inline const ConnectionArray &getConnections() const {
		return mConnections;
	}

	/** Send a message to all connections. */
	inline int sendMsg(const Message &msg) {
		return pomp_ctx_send_msg(mCtx, msg.getMsg());
	}

	/** Send a message on dgram context to a remote address. */
	inline int sendMsgTo(const Message &msg, const struct sockaddr *addr, uint32_t addrlen) {
		return pomp_ctx_send_msg_to(mCtx, msg.getMsg(), addr, addrlen);
	}

	/** Format and send a message to all connections. */
	inline int send(uint32_t msgid, const char *fmt, ...) POMP_ATTRIBUTE_FORMAT_PRINTF(3, 4) {
		va_list args;
		va_start(args, fmt);
		int res = pomp_ctx_sendv(mCtx, msgid, fmt, args);
		va_end(args);
		return res;
	}

	/** Format and send a message to all connections. */
	inline int sendv(uint32_t msgid, const char *fmt, va_list args) {
		return pomp_ctx_sendv(mCtx, msgid, fmt, args);
	}

#ifdef POMP_CXX11
	/** Format and send a message to all connections. */
	template<typename Fmt, typename... ArgsW>
	inline int send(const ArgsW&... args) {
		Message msg;
		int res = msg.write<Fmt>(args...);
		if (res == 0)
			res = sendMsg(msg);
		return res;
	}
#endif /* POMP_CXX11 */
};

} /* namespace pomp */

#endif /* !_LIBPOMP_HPP_ */
