/**
 * @file pomp_encoder.c
 *
 * @brief Handle message payload encoding.
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

#include "pomp_priv.h"

/* Integer flag found in format specifier */
#define FLAG_L	0x01	/**< %l format specifier */
#define FLAG_LL	0x02	/**< %ll format specifier */
#define FLAG_H	0x04	/**< %h format specifier */
#define FLAG_HH	0x08	/**< %hh format specifier */

/*
 * See documentation in public header.
 */
struct pomp_encoder *pomp_encoder_new(void)
{
	struct pomp_encoder *enc = NULL;
	enc = calloc(1, sizeof(*enc));
	if (enc == NULL)
		return NULL;
	return enc;
}

/*
 * See documentation in public header.
 */
int pomp_encoder_destroy(struct pomp_encoder *enc)
{
	POMP_RETURN_ERR_IF_FAILED(enc != NULL, -EINVAL);
	free(enc);
	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_encoder_init(struct pomp_encoder *enc, struct pomp_msg *msg)
{
	POMP_RETURN_ERR_IF_FAILED(enc != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(msg != NULL, -EINVAL);
	enc->msg = msg;
	enc->pos = POMP_PROT_HEADER_SIZE;
	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_encoder_clear(struct pomp_encoder *enc)
{
	POMP_RETURN_ERR_IF_FAILED(enc != NULL, -EINVAL);
	enc->msg = NULL;
	enc->pos = 0;
	return 0;
}

/**
 * Write data in message.
 * @param enc : encoder.
 * @param type : data type.
 * @param p : data to write.
 * @param n : data size.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int encoder_write_data(struct pomp_encoder *enc, uint8_t type,
		const void *p, size_t n)
{
	int res = 0;

	/* Write type */
	res = pomp_buffer_writeb(enc->msg->buf, &enc->pos, type);
	if (res < 0)
		return res;

	/* Write data */
	res = pomp_buffer_write(enc->msg->buf, &enc->pos, p, n);
	if (res < 0)
		return res;

	return 0;
}

/**
 * Write an integer as a variable number of bytes.
 * @param enc : encoder.
 * @param type : data type, 0 to just write raw data without type byte.
 * @param v : value to write.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int encoder_write_varint(struct pomp_encoder *enc, uint8_t type,
		uint64_t v)
{
	uint8_t d[10];
	uint32_t n = 0;
	uint8_t b = 0;
	int more = 0;

	/* Process value, use logical right shift without sign propagation */
	do {
		b = v & 0x7f;
		v >>= 7;
		more = (v != 0);
		if (more)
			b |= 0x80;
		d[n++] = b;
	} while (more);

	/* Write encoded data */
	if (type != 0)
		return encoder_write_data(enc, type, d, n);
	else
		return pomp_buffer_write(enc->msg->buf, &enc->pos, d, n);
}

/**
 * Write size field as u16.
 * @param enc : encoder.
 * @param n : size to write.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int encoder_write_size_u16(struct pomp_encoder *enc, uint16_t n)
{
	uint64_t d = n;
	return encoder_write_varint(enc, 0, d);
}

/**
 * Write size field as u32.
 * @param enc : encoder.
 * @param n : size to write.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int encoder_write_size_u32(struct pomp_encoder *enc, uint32_t n)
{
	uint64_t d = n;
	return encoder_write_varint(enc, 0, d);
}

/**
 * Extract an argument from either a va_list or an array of string arguments
 * _type : type of argument, will be stored in 'v' union.
 * _casttype : C type of argument for casting in 'v' union.
 * _vatype : C type of argument for va_arg extraction.
 * _convfct : conversion function (strtoll like)
 *
 * In case of error it will 'goto out'.
 * It uses several caller context variables
 */
/* codecheck_ignore[COMPLEX_MACRO] */
#define EXTRACT_ARG(_type, _casttype, _vatype, _convfct) \
	if (argv == NULL) { \
		v._type = (_casttype)(va_arg(args, _vatype)); \
	} else if (argidx >= argc || argv[argidx] == NULL) { \
		res = -EINVAL; \
		POMP_LOGW("Missing %s argument", #_type); \
		goto out; \
	} else { \
		v._type = (_casttype)_convfct(argv[argidx], NULL, 0); \
		argidx++; \
	}

/**
 * Macro to be used in EXTRACT_ARG that wrap strtof with an extra arg
 * compatible with strtoll like functions
 */
#define wrap_strtof(_nptr, _endptr, _unused) \
	strtof(_nptr, _endptr)

/**
 * Macro to be used in EXTRACT_ARG that wrap strtod with an extra arg
 * compatible with strtoll like functions
 */
#define wrap_strtod(_nptr, _endptr, _unused) \
	strtod(_nptr, _endptr)

/**
 * Internal write
 * @param decodepos : string to decode.
 * @param len : len in bytes of the resulting string.
 * @param out : allocated output buffer.
 */
static int parse_buffer_argv(const char *decodepos, uint32_t len, void *out)
{
	uint32_t i = 0;
	/* first byte should be padded with 0 if len is odd */
	if (strlen(decodepos) % 2 == 1)
		sscanf(decodepos, "%hhx", &(((uint8_t *)out)[0]));
	else
		sscanf(decodepos, "%2hhx", &(((uint8_t *)out)[0]));
	decodepos += 2;
	for (i = 1; i < len; i++) {
		sscanf(decodepos, "%2hhx", &(((uint8_t *)out)[i]));
		decodepos += 2;
	}
	return 0;
}

/**
 * Internal write
 * @param enc : encoder.
 * @param fmt : format string. Can be NULL if no arguments given.
 * @param argc : number of arguments in argv if args is NULL.
 * @param argv : array of arguments if args is NULL.
 * @param args : arguments if argc, argv is not used
 * @return 0 in case of success, negative errno value in case of error.
 */
static int encoder_writev_internal(struct pomp_encoder *enc, const char *fmt,
		int argc, const char * const *argv, va_list args)
{
	int res = 0;
	int flags = 0;
	char c = 0;
	uint32_t len = 0;
	int argidx = 0;
	union pomp_value v;

	POMP_RETURN_ERR_IF_FAILED(enc != NULL, -EINVAL);

	/* Allow NULL format string, simply return immediately */
	if (fmt == NULL)
		return 0;

	while (res == 0 && *fmt != '\0') {
		/* Only formatting spec expected here */
		c = *fmt++;
		if (c != '%') {
			POMP_LOGW("encoder : invalid format char (%c)", c);
			return -EINVAL;
		}
		flags = 0;

again:
		c = *fmt++;
		switch (c) {
		case 'l':
			if (*fmt == 'l') {
				fmt++;
				flags |= FLAG_LL;
			} else {
				flags |= FLAG_L;
			}
			goto again;

		case 'h':
			if (*fmt == 'h') {
				fmt++;
				flags |= FLAG_HH;
			} else {
				flags |= FLAG_H;
			}
			goto again;

#ifdef _WIN32
		case 'I':
			if (*fmt == '6' && *(fmt + 1) == '4') {
				fmt += 2;
				flags |= FLAG_LL;
				goto again;
			}
			POMP_LOGW("encoder : invalid format specifier (%c)", c);
			res = -EINVAL;
			break;
#endif /* _WIN32 */

		/* Signed integer */
		case 'i': /* NO BREAK */
		case 'd':
			if (flags & FLAG_LL) {
				/* codecheck_ignore[LONG_LINE] */
				EXTRACT_ARG(i64, int64_t, signed long long int, strtoll);
				res = pomp_encoder_write_i64(enc, v.i64);
			} else if (flags & FLAG_L) {
#if defined(__WORDSIZE) && (__WORDSIZE == 64)
				/* codecheck_ignore[LONG_LINE] */
				EXTRACT_ARG(i64, int64_t, signed long int, strtol);
				res = pomp_encoder_write_i64(enc, v.i64);
#else
				/* codecheck_ignore[LONG_LINE] */
				EXTRACT_ARG(i32, int32_t, signed long int, strtol);
				res = pomp_encoder_write_i32(enc, v.i32);
#endif
			} else if (flags & FLAG_HH) {
				EXTRACT_ARG(i8, int8_t, signed int, strtol);
				res = pomp_encoder_write_i8(enc, v.i8);
			} else if (flags & FLAG_H) {
				EXTRACT_ARG(i16, int16_t, signed int, strtol);
				res = pomp_encoder_write_i16(enc, v.i16);
			} else {
				EXTRACT_ARG(i32, int32_t, signed int, strtol);
				res = pomp_encoder_write_i32(enc, v.i32);
			}
			break;

		/* Unsigned integer */
		case 'u':
			if (flags & FLAG_LL) {
				/* codecheck_ignore[LONG_LINE] */
				EXTRACT_ARG(u64, uint64_t, unsigned long long int, strtoull);
				res = pomp_encoder_write_u64(enc, v.u64);
			} else if (flags & FLAG_L) {
#if defined(__WORDSIZE) && (__WORDSIZE == 64)
				/* codecheck_ignore[LONG_LINE] */
				EXTRACT_ARG(u64, uint64_t, unsigned long int, strtoul);
				res = pomp_encoder_write_u64(enc, v.u64);
#else
				/* codecheck_ignore[LONG_LINE] */
				EXTRACT_ARG(u32, uint32_t, unsigned long int, strtoul);
				res = pomp_encoder_write_u32(enc, v.u32);
#endif
			} else if (flags & FLAG_HH) {
				EXTRACT_ARG(u8, uint8_t, unsigned int, strtoul);
				res = pomp_encoder_write_u8(enc, v.u8);
			} else if (flags & FLAG_H) {
				/* codecheck_ignore[LONG_LINE] */
				EXTRACT_ARG(u16, uint16_t, unsigned int, strtoul);
				res = pomp_encoder_write_u16(enc, v.u16);
			} else {
				/* codecheck_ignore[LONG_LINE] */
				EXTRACT_ARG(u32, uint32_t, unsigned int, strtoul);
				res = pomp_encoder_write_u32(enc, v.u32);
			}
			break;

		/* String */
		case 's':
			if (argv == NULL) {
				v.cstr = va_arg(args, const char *);
			} else if (argidx >= argc || argv[argidx] == NULL) {
				res = -EINVAL;
				POMP_LOGW("Missing str argument");
				goto out;
			} else {
				v.cstr = argv[argidx++];
			}
			res = pomp_encoder_write_str(enc, v.cstr);
			break;

		/* Buffer */
		case 'p':
			if (argv != NULL) {
				if (*fmt++ != '%' || *fmt++ != 'u') {
					/* Size expected after pointer */
					/* codecheck_ignore[LONG_LINE] */
					POMP_LOGW("encoder : expected %%u after %%p");
					res = -EINVAL;
				} else {
					int argbufferpos = argidx;
					const char *decodepos;
					/*go to next argument to get size*/
					argidx++;
					/* codecheck_ignore[LONG_LINE] */
					EXTRACT_ARG(u32, uint32_t, unsigned int, strtoul);
					len = v.u32;

					v.buf = malloc(len);
					if (v.buf == NULL) {
						res = -ENOMEM;
						break;
					}
					decodepos = argv[argbufferpos];
					/* codecheck_ignore[LONG_LINE] */
					parse_buffer_argv(decodepos, len, v.buf);
					/* codecheck_ignore[LONG_LINE] */
					res = pomp_encoder_write_buf(enc, v.buf, len);
					free(v.buf);
				}
			} else if (*fmt++ != '%' || *fmt++ != 'u') {
				/* Size expected after pointer */
				POMP_LOGW("encoder : expected %%u after %%p");
				res = -EINVAL;
			} else {
				v.cbuf = va_arg(args, const void *);
				len = va_arg(args, unsigned int);
				res = pomp_encoder_write_buf(enc, v.cbuf, len);
			}
			break;

		/* Floating point */
		case 'f': /* NO BREAK */
		case 'F': /* NO BREAK */
		case 'e': /* NO BREAK */
		case 'E': /* NO BREAK */
		case 'g': /* NO BREAK */
		case 'G':
			if (flags & (FLAG_LL | FLAG_H | FLAG_HH)) {
				POMP_LOGW("encoder : unsupported format width");
				res = -EINVAL;
			} else if (flags & FLAG_L) {
				EXTRACT_ARG(f64, double, double, wrap_strtod);
				res = pomp_encoder_write_f64(enc, v.f64);
			} else {
				/* float shall be extracted as double */
				EXTRACT_ARG(f32, float, double, wrap_strtof);
				res = pomp_encoder_write_f32(enc, v.f32);
			}
			break;

		/* File descriptor (hack) */
		case 'x':
			if (flags & (FLAG_LL | FLAG_L | FLAG_H | FLAG_HH)) {
				POMP_LOGW("encoder : unsupported format width");
				res = -EINVAL;
			} else {
				EXTRACT_ARG(fd, int, int, strtol);
				res = pomp_encoder_write_fd(enc, v.fd);
			}
			break;

		default:
			POMP_LOGW("encoder : invalid format specifier (%c)", c);
			res = -EINVAL;
			break;
		}
	}

out:
	return res;
}

/**
 * Internal write
 * @param enc : encoder.
 * @param fmt : format string. Can be NULL if no arguments given.
 * @param argc : number of arguments in argv if args is NULL.
 * @param argv : array of arguments if args is NULL.
 * @param ... : arguments if argc, argv is not used
 * @return 0 in case of success, negative errno value in case of error.
 *
 * @remarks needed because pomp_encoder_write_argv cannot pass NULL as va_list.
 */
static int encoder_write_internal(struct pomp_encoder *enc, const char *fmt,
		int argc, const char * const *argv, ...)
{
	int res = 0;
	va_list args;
	va_start(args, argv);
	res = encoder_writev_internal(enc, fmt, argc, argv, args);
	va_end(args);
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_encoder_write(struct pomp_encoder *enc, const char *fmt, ...)
{
	int res = 0;
	va_list args;
	va_start(args, fmt);
	res = encoder_writev_internal(enc, fmt, 0, NULL, args);
	va_end(args);
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_encoder_writev(struct pomp_encoder *enc, const char *fmt, va_list args)
{
	return encoder_writev_internal(enc, fmt, 0, NULL, args);
}

/*
 * See documentation in public header.
 */
int pomp_encoder_write_argv(struct pomp_encoder *enc,
		const char *fmt, int argc, const char * const *argv)
{
	/* Nothing to do if no arguments */
	if (argc == 0)
		return 0;

	/* make sure array is not NULL if some args are present */
	POMP_RETURN_ERR_IF_FAILED(argv != NULL, -EINVAL);

	/* Need to go though intermediate function because we can't pass NULL
	 * as va_list */
	return encoder_write_internal(enc, fmt, argc, argv);
}

/*
 * See documentation in public header.
 */
int pomp_encoder_write_i8(struct pomp_encoder *enc, int8_t v)
{
	uint8_t d = (uint8_t)v;
	POMP_RETURN_ERR_IF_FAILED(enc != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(enc->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(!enc->msg->finished, -EPERM);
	return encoder_write_data(enc, POMP_PROT_DATA_TYPE_I8, &d, sizeof(d));
}

/*
 * See documentation in public header.
 */
int pomp_encoder_write_u8(struct pomp_encoder *enc, uint8_t v)
{
	uint8_t d = v;
	POMP_RETURN_ERR_IF_FAILED(enc != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(enc->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(!enc->msg->finished, -EPERM);
	return encoder_write_data(enc, POMP_PROT_DATA_TYPE_U8, &d, sizeof(d));
}

/*
 * See documentation in public header.
 */
int pomp_encoder_write_i16(struct pomp_encoder *enc, int16_t v)
{
	uint16_t d = POMP_HTOLE16(v);
	POMP_RETURN_ERR_IF_FAILED(enc != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(enc->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(!enc->msg->finished, -EPERM);
	return encoder_write_data(enc, POMP_PROT_DATA_TYPE_I16, &d, sizeof(d));
}

/*
 * See documentation in public header.
 */
int pomp_encoder_write_u16(struct pomp_encoder *enc, uint16_t v)
{
	uint16_t d = POMP_HTOLE16(v);
	POMP_RETURN_ERR_IF_FAILED(enc != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(enc->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(!enc->msg->finished, -EPERM);
	return encoder_write_data(enc, POMP_PROT_DATA_TYPE_U16, &d, sizeof(d));
}

/*
 * See documentation in public header.
 */
int pomp_encoder_write_i32(struct pomp_encoder *enc, int32_t v)
{
	/* Zigzag encoding, use arithmetic right shift, with sign propagation */
	uint64_t d = (uint32_t)((v << 1) ^ (v >> 31));
	POMP_RETURN_ERR_IF_FAILED(enc != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(enc->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(!enc->msg->finished, -EPERM);
	return encoder_write_varint(enc, POMP_PROT_DATA_TYPE_I32, d);
}

/*
 * See documentation in public header.
 */
int pomp_encoder_write_u32(struct pomp_encoder *enc, uint32_t v)
{
	uint64_t d = v;
	POMP_RETURN_ERR_IF_FAILED(enc != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(enc->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(!enc->msg->finished, -EPERM);
	return encoder_write_varint(enc, POMP_PROT_DATA_TYPE_U32, d);
}

/*
 * See documentation in public header.
 */
int pomp_encoder_write_i64(struct pomp_encoder *enc, int64_t v)
{
	/* Zigzag encoding, use arithmetic right shift, with sign propagation */
	uint64_t d = (uint64_t)((v << 1) ^ (v >> 63));
	POMP_RETURN_ERR_IF_FAILED(enc != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(enc->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(!enc->msg->finished, -EPERM);
	return encoder_write_varint(enc, POMP_PROT_DATA_TYPE_I64, d);
}

/*
 * See documentation in public header.
 */
int pomp_encoder_write_u64(struct pomp_encoder *enc, uint64_t v)
{
	uint64_t d = v;
	POMP_RETURN_ERR_IF_FAILED(enc != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(enc->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(!enc->msg->finished, -EPERM);
	return encoder_write_varint(enc, POMP_PROT_DATA_TYPE_U64, d);
}

/*
 * See documentation in public header.
 */
int pomp_encoder_write_str(struct pomp_encoder *enc, const char *v)
{
	int res = 0;
	size_t len = 0;

	POMP_RETURN_ERR_IF_FAILED(enc != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(enc->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(!enc->msg->finished, -EPERM);
	POMP_RETURN_ERR_IF_FAILED(v != NULL, -EINVAL);

	/* Compute string length, add null byte */
	len = strlen(v) + 1;
	if (len > 0xffff) {
		POMP_LOGW("encoder : invalid string length %u", (uint32_t)len);
		return -EINVAL;
	}

	/* Write type */
	res = pomp_buffer_writeb(enc->msg->buf, &enc->pos,
			POMP_PROT_DATA_TYPE_STR);
	if (res < 0)
		return res;

	/* Write string length */
	res = encoder_write_size_u16(enc, (uint16_t)len);
	if (res < 0)
		return res;

	/* Write string data */
	return pomp_buffer_write(enc->msg->buf, &enc->pos, v, len);
}

/*
 * See documentation in public header.
 */
int pomp_encoder_write_buf(struct pomp_encoder *enc, const void *v, uint32_t n)
{
	int res = 0;

	POMP_RETURN_ERR_IF_FAILED(enc != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(enc->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(!enc->msg->finished, -EPERM);
	POMP_RETURN_ERR_IF_FAILED(v != NULL, -EINVAL);

	/* Write type */
	res = pomp_buffer_writeb(enc->msg->buf, &enc->pos,
			POMP_PROT_DATA_TYPE_BUF);
	if (res < 0)
		return res;

	/* Write length */
	res = encoder_write_size_u32(enc, n);
	if (res < 0)
		return res;

	/* Write data */
	return pomp_buffer_write(enc->msg->buf, &enc->pos, v, n);
}

/*
 * See documentation in public header.
 */
int pomp_encoder_write_f32(struct pomp_encoder *enc, float v)
{
	union {
		float f32;
		uint32_t u32;
	} d;
	POMP_RETURN_ERR_IF_FAILED(enc != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(enc->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(!enc->msg->finished, -EPERM);
	d.f32 = v;
	d.u32 = POMP_HTOLE32(d.u32);
	return encoder_write_data(enc, POMP_PROT_DATA_TYPE_F32, &d, sizeof(d));
}

/*
 * See documentation in public header.
 */
int pomp_encoder_write_f64(struct pomp_encoder *enc, double v)
{
	union {
		double f64;
		uint64_t u64;
	} d;
	POMP_RETURN_ERR_IF_FAILED(enc != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(enc->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(!enc->msg->finished, -EPERM);
	d.f64 = v;
	d.u64 = POMP_HTOLE64(d.u64);
	return encoder_write_data(enc, POMP_PROT_DATA_TYPE_F64, &d, sizeof(d));
}

/*
 * See documentation in public header.
 */
int pomp_encoder_write_fd(struct pomp_encoder *enc, int v)
{
	int res = 0;
	POMP_RETURN_ERR_IF_FAILED(enc != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(enc->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(!enc->msg->finished, -EPERM);

	/* Write type */
	res = pomp_buffer_writeb(enc->msg->buf, &enc->pos,
			POMP_PROT_DATA_TYPE_FD);
	if (res < 0)
		return res;

	/* Write file descriptor */
	return pomp_buffer_write_fd(enc->msg->buf, &enc->pos, v);
}
