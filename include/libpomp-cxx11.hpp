/**
 * @file libpomp-cxx11.hpp
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

#if !defined(_LIBPOMP_HPP_) || !defined(POMP_CXX11)
#  error "This file shall not be included directly, use libpomp.hpp"
#endif

#ifndef _LIBPOMP_CXX11_HPP_
#define _LIBPOMP_CXX11_HPP_

namespace pomp {

/** Argument type */
enum ArgType {
	ArgI8,   /**< 8-bit signed integer */
	ArgU8,   /**< 8-bit unsigned integer */
	ArgI16,  /**< 16-bit signed integer */
	ArgU16,  /**< 16-bit unsigned integer */
	ArgI32,  /**< 32-bit signed integer */
	ArgU32,  /**< 32-bit unsigned integer */
	ArgI64,  /**< 64-bit signed integer */
	ArgU64,  /**< 64-bit unsigned integer */
	ArgStr,  /**< String */
	ArgBuf,  /**< Buffer */
	ArgF32,  /**< 32-bit floating point */
	ArgF64,  /**< 64-bit floating point */
	ArgFd,   /**< File descriptor */
};

namespace internal {

/** Generic argument traits */
template<ArgType T> struct traits {
	enum {valid = false};
	typedef void *type;
	static int encode(struct pomp_encoder *enc, const type &v);
	static int decode(struct pomp_decoder *dec, type &v);
};

/** I8 argument traits */
template<> struct traits<ArgI8> {
	enum {valid = true};
	typedef int8_t type;
	inline static int encode(struct pomp_encoder *enc, const type &v) {
		return pomp_encoder_write_i8(enc, v);
	}
	inline static int decode(struct pomp_decoder *dec, type &v) {
		return pomp_decoder_read_i8(dec, &v);
	}
};

/** U8 argument traits */
template<> struct traits<ArgU8> {
	enum {valid = true};
	typedef uint8_t type;
	inline static int encode(struct pomp_encoder *enc, const type &v) {
		return pomp_encoder_write_u8(enc, v);
	}
	inline static int decode(struct pomp_decoder *dec, type &v) {
		return pomp_decoder_read_u8(dec, &v);
	}
};

/** I16 argument traits */
template<> struct traits<ArgI16> {
	enum {valid = true};
	typedef int16_t type;
	inline static int encode(struct pomp_encoder *enc, const type &v) {
		return pomp_encoder_write_i16(enc, v);
	}
	inline static int decode(struct pomp_decoder *dec, type &v) {
		return pomp_decoder_read_i16(dec, &v);
	}
};

/** U16 argument traits */
template<> struct traits<ArgU16> {
	enum {valid = true};
	typedef uint16_t type;
	inline static int encode(struct pomp_encoder *enc, const type &v) {
		return pomp_encoder_write_u16(enc, v);
	}
	inline static int decode(struct pomp_decoder *dec, type &v) {
		return pomp_decoder_read_u16(dec, &v);
	}
};

/** I32 argument traits */
template<> struct traits<ArgI32> {
	enum {valid = true};
	typedef int32_t type;
	inline static int encode(struct pomp_encoder *enc, const type &v) {
		return pomp_encoder_write_i32(enc, v);
	}
	inline static int decode(struct pomp_decoder *dec, type &v) {
		return pomp_decoder_read_i32(dec, &v);
	}
};

/** U32 argument traits */
template<> struct traits<ArgU32> {
	enum {valid = true};
	typedef uint32_t type;
	inline static int encode(struct pomp_encoder *enc, const type &v) {
		return pomp_encoder_write_u32(enc, v);
	}
	inline static int decode(struct pomp_decoder *dec, type &v) {
		return pomp_decoder_read_u32(dec, &v);
	}
};

/** I64 argument traits */
template<> struct traits<ArgI64> {
	enum {valid = true};
	typedef int64_t type;
	inline static int encode(struct pomp_encoder *enc, const type &v) {
		return pomp_encoder_write_i64(enc, v);
	}
	inline static int decode(struct pomp_decoder *dec, type &v) {
		return pomp_decoder_read_i64(dec, &v);
	}
};

/** U64 argument traits */
template<> struct traits<ArgU64> {
	enum {valid = true};
	typedef uint64_t type;
	inline static int encode(struct pomp_encoder *enc, const type &v) {
		return pomp_encoder_write_u64(enc, v);
	}
	inline static int decode(struct pomp_decoder *dec, type &v) {
		return pomp_decoder_read_u64(dec, &v);
	}
};

/** STR argument traits */
template<> struct traits<ArgStr> {
	enum {valid = true};
	typedef std::string type;
	inline static int encode(struct pomp_encoder *enc, const type &v) {
		return pomp_encoder_write_str(enc, v.c_str());
	}
	inline static int decode(struct pomp_decoder *dec, type &v) {
		const char *s = NULL;
		int res = pomp_decoder_read_cstr(dec, &s);
		if (res == 0)
			v.assign(s);
		return res;
	}
};

/** BUF argument traits */
template<> struct traits<ArgBuf> {
	enum {valid = true};
	typedef std::vector<uint8_t> type;
	inline static int encode(struct pomp_encoder *enc, const type &v) {
		const uint8_t *p = v.data();
		uint32_t n = static_cast<uint32_t>(v.size());
		return pomp_encoder_write_buf(enc, p, n);
	}
	inline static int decode(struct pomp_decoder *dec, type &v) {
		const void *p = NULL;
		uint32_t n = 0;
		int res = pomp_decoder_read_cbuf(dec, &p, &n);
		if (res == 0) {
			const uint8_t *start = reinterpret_cast<const uint8_t *>(p);
			const uint8_t *end = start + n;
			v.assign(start, end);
		}
		return res;
	}
};

/** F32 argument traits */
template<> struct traits<ArgF32> {
	enum {valid = true};
	typedef float type;
	inline static int encode(struct pomp_encoder *enc, const type &v) {
		return pomp_encoder_write_f32(enc, v);
	}
	inline static int decode(struct pomp_decoder *dec, type &v) {
		return pomp_decoder_read_f32(dec, &v);
	}
};

/** F64 argument traits */
template<> struct traits<ArgF64> {
	enum {valid = true};
	typedef double type;
	inline static int encode(struct pomp_encoder *enc, const type &v) {
		return pomp_encoder_write_f64(enc, v);
	}
	inline static int decode(struct pomp_decoder *dec, type &v) {
		return pomp_decoder_read_f64(dec, &v);
	}
};

/** FD argument traits */
template<> struct traits<ArgFd> {
	enum {valid = true};
	typedef int type;
	inline static int encode(struct pomp_encoder *enc, const type &v) {
		return pomp_encoder_write_fd(enc, v);
	}
	inline static int decode(struct pomp_decoder *dec, type &v) {
		return pomp_decoder_read_fd(dec, &v);
	}
};

} /* namespace internal */

/** Message formation specification */
template<uint32_t Id, ArgType... Args>
struct MessageFormat {
	enum {id = Id};
	static int encode(struct pomp_encoder *enc,
			const typename pomp::internal::traits<Args>::type&... args);
	static int decode(struct pomp_decoder *dec,
			typename pomp::internal::traits<Args>::type&... args);
};

/** Specialization with no arguments */
template<uint32_t Id>
struct MessageFormat<Id> {
	enum {id = Id};
	inline static int encode(struct pomp_encoder *enc) { (void)enc; return 0;}
	inline static int decode(struct pomp_decoder *dec) { (void)dec; return 0;}
};

/** Specialization for recursion */
template<uint32_t Id, ArgType Arg1, ArgType... Args>
struct MessageFormat<Id, Arg1, Args...> {
	enum {id = Id};
	typedef MessageFormat<Id, Args...> _Base;

	/** Encode arguments according to format. */
	inline static int encode(struct pomp_encoder *enc,
			const typename pomp::internal::traits<Arg1>::type& arg1,
			const typename pomp::internal::traits<Args>::type&... args) {
		static_assert(pomp::internal::traits<Arg1>::valid, "Invalid type");
		pomp::internal::traits<Arg1>::encode(enc, arg1);
		return _Base::encode(enc, std::forward<
				const typename pomp::internal::traits<Args>::type&>(args)...);
	}

	/** Decode arguments according to format. */
	inline static int decode(struct pomp_decoder *dec,
			typename pomp::internal::traits<Arg1>::type& arg1,
			typename pomp::internal::traits<Args>::type&... args) {
		static_assert(pomp::internal::traits<Arg1>::valid, "Invalid type");
		pomp::internal::traits<Arg1>::decode(dec, arg1);
		return _Base::decode(dec, std::forward<
				typename pomp::internal::traits<Args>::type&>(args)...);
	}
};

} /* namespace pomp */

#endif /* !_LIBPOMP_CXX11_HPP_ */
