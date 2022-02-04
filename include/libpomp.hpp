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
#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

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
class Event;
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
	friend class Decoder;
	friend class Encoder;

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
			(void)pomp_msg_destroy(mMsg);
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
 * Decoder class.
 */
class Decoder {
	POMP_DISABLE_COPY(Decoder)
private:
	const Message &mMsg; /**< Decoded message */
	pomp_decoder *mDec;  /**< Internal decoder */

public:
	Decoder(const Message &message) :
		mMsg(message),
		mDec(pomp_decoder_new()) {
		pomp_decoder_init(mDec, mMsg.getMsg());
	}

	~Decoder() {
		pomp_decoder_destroy(mDec);
	}

	int read(int8_t &v) {
		return pomp_decoder_read_i8(mDec, &v);
	}

	int read(uint8_t &v) {
		return pomp_decoder_read_u8(mDec, &v);
	}

	int read(int16_t &v) {
		return pomp_decoder_read_i16(mDec, &v);
	}

	int read(uint16_t &v) {
		return pomp_decoder_read_u16(mDec, &v);
	}

	int read(int32_t &v) {
		return pomp_decoder_read_i32(mDec, &v);
	}

	int read(uint32_t &v) {
		return pomp_decoder_read_u32(mDec, &v);
	}

	int read(int64_t &v) {
		return pomp_decoder_read_i64(mDec, &v);
	}

	int read(uint64_t &v) {
		return pomp_decoder_read_u64(mDec, &v);
	}

	int read(float &v) {
		return pomp_decoder_read_f32(mDec, &v);
	}

	int read(double &v) {
		return pomp_decoder_read_f64(mDec, &v);
	}

	int read(std::string &v) {
		const char *s = NULL;
		int res = pomp_decoder_read_cstr(mDec, &s);
		if (res == 0)
			v.assign(s);
		return res;
	}

	int read(std::vector<uint8_t> &v) {
		const void *p = NULL;
		uint32_t n = 0;
		int res = pomp_decoder_read_cbuf(mDec, &p, &n);
		if (res == 0) {
			const uint8_t *start = reinterpret_cast<const uint8_t *>(p);
			const uint8_t *end = start + n;
			v.assign(start, end);
		}
		return res;
	}

	//dedicated function to avoid conflict with read(int32_t &)
	int readFd(int &v) {
		return pomp_decoder_read_fd(mDec, &v);
	}
};

/**
 * Encoder class.
 *
 * Use writeXX functions to avoid cast when using numeric constants.
 */
class Encoder {
	POMP_DISABLE_COPY(Encoder)
private:
	Message mMsg;	   /**< Encoded message */
	pomp_encoder *mEnc; /**< Internal encoder */
	bool mFinished;	 /**< Is message encoding finished */

public:
	Encoder(uint32_t msgId) :
		mEnc(pomp_encoder_new()),
		mFinished(false) {
		pomp_msg_init(mMsg.mMsg, msgId);
		pomp_encoder_init(mEnc, mMsg.mMsg);
	}
	~Encoder() {
		pomp_encoder_destroy(mEnc);
	}

	const Message &getMessage() {
		if (!mFinished) {
			pomp_msg_finish(mMsg.mMsg);
			mFinished = true;
		}
		return mMsg;
	}

	int writeI8(int8_t v) { return write(v); }
	int write(int8_t v) {
		return pomp_encoder_write_i8(mEnc, v);
	}

	int writeU8(uint8_t v) { return write(v); }
	int write(uint8_t v) {
		return pomp_encoder_write_u8(mEnc, v);
	}

	int writeI16(int16_t v) { return write(v); }
	int write(int16_t v) {
		return pomp_encoder_write_i16(mEnc, v);
	}

	int writeU16(uint16_t v) { return write(v); }
	int write(uint16_t v) {
		return pomp_encoder_write_u16(mEnc, v);
	}

	int writeI32(int32_t v) { return write(v); }
	int write(int32_t v) {
		return pomp_encoder_write_i32(mEnc, v);
	}

	int writeU32(uint32_t v) { return write(v); }
	int write(uint32_t v) {
		return pomp_encoder_write_u32(mEnc, v);
	}

	int writeI64(int64_t v) { return write(v); }
	int write(int64_t v) {
		return pomp_encoder_write_i64(mEnc, v);
	}

	int writeU64(uint64_t v) { return write(v); }
	int write(uint64_t v) {
		return pomp_encoder_write_u64(mEnc, v);
	}

	int writeF32(float v) { return write(v); }
	int write(float v) {
		return pomp_encoder_write_f32(mEnc, v);
	}

	int writeF64(double v) { return write(v); }
	int write(double v) {
		return pomp_encoder_write_f64(mEnc, v);
	}

	int write(const std::string &v) { return write(v.c_str()); }
	int write(const char *v) {
		return pomp_encoder_write_str(mEnc, v);
	}

	int write(const std::vector<uint8_t> &v) {
		const uint8_t *p = v.data();
		uint32_t n = static_cast<uint32_t>(v.size());
		return pomp_encoder_write_buf(mEnc, p, n);
	}

	//dedicated function to avoid conflict with write(int32_t)
	int writeFd(int fd) {
		return pomp_encoder_write_fd(mEnc, fd);
	}
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

	/** Send a buffer to the peer of the raw connection. */
	inline int send(const std::vector<uint8_t> &v) {
		int res;
		struct pomp_buffer *buf;
		buf = pomp_buffer_new_with_data(v.data(), v.size());
		if (buf != NULL) {
			res = pomp_conn_send_raw_buf(mConn, buf);
			pomp_buffer_unref(buf);
			return res;
		}
		return -ENOMEM;
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
	friend class Event;
private:
	/** Internal fd event callback */
	inline static void fdEventCb(int _fd, uint32_t _revents, void *_userdata) {
		Handler *handler = reinterpret_cast<Handler *>(_userdata);
		handler->processFd(_fd, _revents);
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

	inline Loop(struct pomp_loop *loop) {
		mLoop = loop;
		mOwner = false;
	}

	/** Destructor. */
	inline ~Loop() {
		if (mOwner)
			(void)pomp_loop_destroy(mLoop);
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

	/** Get internal pomp_loop (cast operator) */
	inline operator struct pomp_loop *() {
		return mLoop;
	}

	/** Get internal pomp_loop (explicit getter) */
	inline struct pomp_loop *get() const {
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
 * Event class.
 */
class Event {
	POMP_DISABLE_COPY(Event)
private:
	struct pomp_evt *mEvt;  /**< Internal event */
private:
	/** Internal event callback */
	inline static void eventCb(struct pomp_evt *_evt, void *_userdata) {
		(void)_evt;
		Handler *handler = reinterpret_cast<Handler *>(_userdata);
		handler->processEvent();
	}

public:
	/** Handler class */
	class Handler {
		POMP_DISABLE_COPY(Handler)
	public:
		inline Handler() {}
		inline virtual ~Handler() {}
		virtual void processEvent() = 0;
	};

	/** Constructor */
	inline Event() {
		mEvt = pomp_evt_new();
	}

	/** Destructor */
	inline ~Event() {
		(void)pomp_evt_destroy(mEvt);
	}

	inline int signal() {
		return pomp_evt_signal(mEvt);
	}

	inline int clear() {
		return pomp_evt_clear(mEvt);
	}

	inline int attachToLoop(Loop *loop, Handler *handler) {
		return pomp_evt_attach_to_loop(mEvt, loop->mLoop,
					&Event::eventCb, handler);
	}

	inline int detachFromLoop(Loop *loop) {
		return pomp_evt_detach_from_loop(mEvt, loop->mLoop);
	}

	inline bool isAttached(Loop *loop) {
		struct pomp_loop *_loop = loop ? loop->mLoop : NULL;
		return pomp_evt_is_attached(mEvt, _loop) != 0;
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
		inline virtual void processEvent() {mFunc();}
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
		(void)pomp_timer_destroy(mTimer);
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
 * Pomp address.
 * Format of string is:
 * - inet:<host>:<port> : ipv4 address with host name and port.
 * - inet6:<host>:<port> : ipv6 address with host name and port
 * - unix:<pathname> : unix local address with file system name.
 * - unix:@<name> : unix local address with abstract name.
 */
class Address
{
public:
	Address() : mAddressLength(0), mValid(false) {}
	Address(const char * str) :
		mAddressLength(sizeof(mAddress)),
		mValid(pomp_addr_parse(str, &mAddress.sa, &mAddressLength) == 0)
	{
	}
	bool isValid() const { return mValid; }
	const sockaddr * addr() const { return &mAddress.sa; }
	uint32_t len() const { return mAddressLength; }
	inline static int getRealAddr(const char * str, std::string &dst)
	{
		char *s = NULL;
		int res = pomp_addr_get_real_addr(str, &s);
		if (res == 0) {
			dst.assign(s);
			free(s);
		}
		return res;
	}
#ifdef POMP_CXX11
	explicit operator bool () const { return isValid(); }
#endif

private:
	union { //ensure we have enough space for all formats supported by pomp
		sockaddr sa;
		sockaddr_storage sa_storage;
	} mAddress;
	uint32_t mAddressLength;
	const bool mValid;
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
	inline virtual void recvRawBuffer(Context *ctx, Connection *conn, const std::vector<uint8_t> &v) { (void)ctx; (void)conn; (void)v; }
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

	/** Internal raw callback */
	inline static void rawCb(struct pomp_ctx *_ctx,
			struct pomp_conn *_conn,
			struct pomp_buffer *_buf,
			void *_userdata) {
		(void)_ctx;
		std::vector<uint8_t> v;
		const void *cdata = NULL;
		size_t len;
		size_t capacity;
		int res;

		/* Get our own object from user data */
		Context *self = reinterpret_cast<Context *>(_userdata);

		Connection *conn = NULL;
		ConnectionArray::iterator it;
		it = self->findConn(_conn);
		if (it != self->mConnections.end())
			conn = *it;

		res = pomp_buffer_get_cdata(_buf, &cdata, &len, &capacity);
		if (res == 0) {
			const uint8_t *start = reinterpret_cast<const uint8_t *>(cdata);
			const uint8_t *end = start + len;
			v.assign(start, end);
			self->mEventHandler->recvRawBuffer(self, conn, v);
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
		(void)pomp_ctx_destroy(mCtx);
		if (mLoop != NULL && !mExtLoop)
			delete mLoop;
	}

	/** Start a server. */
	inline int listen(const struct sockaddr *addr, uint32_t addrlen) {
		return pomp_ctx_listen(mCtx, addr, addrlen);
	}

	/** Start a server. */
	inline int listen(const Address &address) {
		return listen(address.addr(), address.len());
	}

	/** Start a server with unix socket address access mode. */
	inline int listen(const struct sockaddr *addr, uint32_t addrlen, uint32_t mode) {
		return pomp_ctx_listen_with_access_mode(mCtx, addr, addrlen, mode);
	}

	/** Start a server with unix socket address access mode. */
	inline int listen(const Address &address, uint32_t mode) {
		return listen(address.addr(), address.len(), mode);
	}

	/** Mark the context as raw */
	inline int setRaw() {
		return pomp_ctx_set_raw(mCtx, &Context::rawCb);
	}

	/** Start a client. */
	inline int connect(const struct sockaddr *addr, uint32_t addrlen) {
		return pomp_ctx_connect(mCtx, addr, addrlen);
	}

	/** Start a client. */
	inline int connect(const Address &address) {
		return connect(address.addr(), address.len());
	}

	/** Bind a connection-less context (inet-udp). */
	inline int bind(const struct sockaddr *addr, uint32_t addrlen) {
		return pomp_ctx_bind(mCtx, addr, addrlen);
	}

	/** Bind a connection-less context (inet-udp). */
	inline int bind(const Address & address) {
		return bind(address.addr(), address.len());
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

	/** Wakeup the context from another thread. */
	inline int wakeup() {
		return pomp_ctx_wakeup(mCtx);
	}

	/** Get context local address (for server or udp context started with listen or bind ). */
	inline const struct sockaddr *getLocalAddr(uint32_t *addrlen) {
		return pomp_ctx_get_local_addr(mCtx, addrlen);
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

	/** Send a message on dgram context to a remote address. */
	inline int sendMsgTo(const Message &msg, const Address & address) {
		return sendMsgTo(msg, address.addr(), address.len());
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

	/** Send a buffer to all connections. */
	inline int send(const std::vector<uint8_t> &v) {
		int res;
		struct pomp_buffer *buf;
		buf = pomp_buffer_new_with_data(v.data(), v.size());
		if (buf != NULL) {
			res = pomp_ctx_send_raw_buf(mCtx, buf);
			pomp_buffer_unref(buf);
			return res;
		}
		return -ENOMEM;
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
