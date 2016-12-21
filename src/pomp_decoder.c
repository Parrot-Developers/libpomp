/**
 * @file pomp_decoder.c
 *
 * @brief Handle message payload decoding.
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

/**
 * Maximum number of strings that can be decoded in the scanf-like function.
 * This limits is due to the static size of array used for saving allocated
 * strings.
 */
#define MAX_DECODE_STR	16

/* Decoding flags */
#define FLAG_L	0x01	/**< %l format specifier */
#define FLAG_LL	0x02	/**< %ll format specifier */
#define FLAG_H	0x04	/**< %h format specifier */
#define FLAG_HH	0x08	/**< %hh format specifier */
#define FLAG_M	0x10	/**< %m format specifier */

/*
 * See documentation in public header.
 */
struct pomp_decoder *pomp_decoder_new(void)
{
	struct pomp_decoder *dec = NULL;
	dec = calloc(1, sizeof(*dec));
	if (dec == NULL)
		return NULL;
	return dec;
}

/*
 * See documentation in public header.
 */
int pomp_decoder_destroy(struct pomp_decoder *dec)
{
	POMP_RETURN_ERR_IF_FAILED(dec != NULL, -EINVAL);
	free(dec);
	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_decoder_init(struct pomp_decoder *dec, const struct pomp_msg *msg)
{
	POMP_RETURN_ERR_IF_FAILED(dec != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(msg != NULL, -EINVAL);
	dec->msg = msg;
	dec->pos = POMP_PROT_HEADER_SIZE;
	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_decoder_clear(struct pomp_decoder *dec)
{
	POMP_RETURN_ERR_IF_FAILED(dec != NULL, -EINVAL);
	dec->msg = NULL;
	dec->pos = 0;
	return 0;
}

/**
 * Read data from message.
 * @param dec : decoder.
 * @param type : expected data type of next encoded argument.
 * @param p : data to read.
 * @param n : data size.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int decoder_read_data(struct pomp_decoder *dec, uint8_t type,
		void *p, size_t n)
{
	int res = 0;
	uint8_t readtype = 0;

	/* Read type */
	res = pomp_buffer_readb(dec->msg->buf, &dec->pos, &readtype);
	if (res < 0)
		return res;

	/* Check type, rewind in case of mismatch */
	if (readtype != type) {
		POMP_LOGW("decoder : type mismatch %d(%d)", readtype, type);
		dec->pos -= sizeof(uint8_t);
		return -EINVAL;
	}

	/* Read data */
	res = pomp_buffer_read(dec->msg->buf, &dec->pos, p, n);
	if (res < 0)
		return res;

	return 0;
}

/**
 * Read an integer as a variable number of bytes.
 * @param dec : decoder.
 * @param type : data type, 0 to just read raw data without type byte.
 * @param v : value to read.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int decoder_read_varint(struct pomp_decoder *dec, uint8_t type,
		uint64_t *v)
{
	int res = 0;
	uint8_t readtype = 0;
	uint8_t b = 0;
	uint32_t shift = 0;

	/* Read type */
	if (type != 0) {
		res = pomp_buffer_readb(dec->msg->buf, &dec->pos, &readtype);
		if (res < 0)
			return res;

		/* Check type, rewind in case of mismatch */
		if (readtype != type) {
			POMP_LOGW("decoder : type mismatch %d(%d)",
					readtype, type);
			dec->pos -= sizeof(uint8_t);
			return -EINVAL;
		}
	}

	/* Decode value */
	*v = 0;
	do {
		res = pomp_buffer_readb(dec->msg->buf, &dec->pos, &b);
		if (res < 0)
			return res;
		*v |= (((uint64_t)(b & 0x7f)) << shift);
		shift += 7;
	} while (b & 0x80);

	return 0;
}

/**
 * Read size field as u16.
 * @param dec : decoder.
 * @param n : size to read.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int decoder_read_size_u16(struct pomp_decoder *dec, uint16_t *n)
{
	int res = 0;
	uint64_t d = 0;
	res = decoder_read_varint(dec, 0, &d);
	*n = (uint16_t)d;
	return res;
}

/**
 * Read size field as u32.
 * @param dec : decoder.
 * @param n : size to read.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int decoder_read_size_u32(struct pomp_decoder *dec, uint32_t *n)
{
	int res = 0;
	uint64_t d = 0;
	res = decoder_read_varint(dec, 0, &d);
	*n = (uint32_t)d;
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_decoder_read(struct pomp_decoder *dec, const char *fmt, ...)
{
	int res = 0;
	va_list args;
	va_start(args, fmt);
	res = pomp_decoder_readv(dec, fmt, args);
	va_end(args);
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_decoder_readv(struct pomp_decoder *dec, const char *fmt, va_list args)
{
	int res = 0;
	int flags = 0;
	char c = 0;
	uint32_t len = 0;
	union pomp_value v;
	char **strsav[MAX_DECODE_STR];
	size_t strsavcount = 0, i = 0;

	POMP_RETURN_ERR_IF_FAILED(dec != NULL, -EINVAL);

	/* Allow NULL format string, simply return immediately */
	if (fmt == NULL)
		return 0;

	memset(strsav, 0, sizeof(strsav));
	while (*fmt != '\0') {
		/* Only formatting spec expected here */
		c = *fmt++;
		if (c != '%') {
			POMP_LOGW("decoder : invalid format char (%c)", c);
			res = -EINVAL;
			goto error;
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

		case 'm':
			flags |= FLAG_M;
			goto again;

#ifdef _WIN32
		case 'I':
			if (*fmt == '6' && *(fmt + 1) == '4') {
				fmt += 2;
				flags |= FLAG_LL;
				goto again;
			}
			POMP_LOGW("decoder : invalid format specifier (%c)", c);
			res = -EINVAL;
			break;
#endif /* _WIN32 */

		/* Signed integer */
		case 'i': /* NO BREAK */
		case 'd':
			if (flags & FLAG_LL) {
				res = pomp_decoder_read_i64(dec, &v.i64);
				if (res < 0)
					goto error;
				*va_arg(args, signed long long int *) = v.i64;
			} else if (flags & FLAG_L) {
#if defined(__WORDSIZE) && (__WORDSIZE == 64)
				res = pomp_decoder_read_i64(dec, &v.i64);
				if (res < 0)
					goto error;
				*va_arg(args, signed long int *) = v.i64;
#else
				res = pomp_decoder_read_i32(dec, &v.i32);
				if (res < 0)
					goto error;
				*va_arg(args, signed long int *) = v.i32;
#endif
			} else if (flags & FLAG_HH) {
				res = pomp_decoder_read_i8(dec, &v.i8);
				if (res < 0)
					goto error;
				*va_arg(args, signed char *) = v.i8;
			} else if (flags & FLAG_H) {
				res = pomp_decoder_read_i16(dec, &v.i16);
				if (res < 0)
					goto error;
				*va_arg(args, signed short *) = v.i16;
			} else {
				res = pomp_decoder_read_i32(dec, &v.i32);
				if (res < 0)
					goto error;
				*va_arg(args, signed int *) = v.i32;
			}
			break;

		/* Unsigned integer */
		case 'u':
			if (flags & FLAG_LL) {
				res = pomp_decoder_read_u64(dec, &v.u64);
				if (res < 0)
					goto error;
				*va_arg(args, unsigned long long int *) = v.u64;
			} else if (flags & FLAG_L) {
#if defined(__WORDSIZE) && (__WORDSIZE == 64)
				res = pomp_decoder_read_u64(dec, &v.u64);
				if (res < 0)
					goto error;
				*va_arg(args, unsigned long int *) = v.u64;
#else
				res = pomp_decoder_read_u32(dec, &v.u32);
				if (res < 0)
					goto error;
				*va_arg(args, unsigned long int *) = v.u32;
#endif
			} else if (flags & FLAG_HH) {
				res = pomp_decoder_read_u8(dec, &v.u8);
				if (res < 0)
					goto error;
				*va_arg(args, unsigned char *) = v.u8;
			} else if (flags & FLAG_H) {
				res = pomp_decoder_read_u16(dec, &v.u16);
				if (res < 0)
					goto error;
				*va_arg(args, unsigned short *) = v.u16;
			} else {
				res = pomp_decoder_read_u32(dec, &v.u32);
				if (res < 0)
					goto error;
				*va_arg(args, unsigned int *) = v.u32;
			}
			break;

		/* String */
		case 's':
			if (!(flags & FLAG_M)) {
				/* Only dynamically allocated string allowed */
				POMP_LOGW("decoder : use %%ms instead of %%s");
				res = -EINVAL;
				goto error;
			} else if (strsavcount == MAX_DECODE_STR) {
				/* Too may strings decoded */
				POMP_LOGW("decoder : too many strings");
				res = -E2BIG;
				goto error;
			} else {
				res = pomp_decoder_read_str(dec, &v.str);
				if (res < 0)
					goto error;
				/* Save address where we stored the allocated
				 * string so we can cleanup in case of error */
				strsav[strsavcount] = va_arg(args, char **);
				*strsav[strsavcount] = v.str;
				strsavcount++;
			}
			break;

		/* Buffer */
		case 'p':
			/* Size expected after pointer */
			if (*fmt++ != '%' || *fmt++ != 'u') {
				POMP_LOGW("decoder : expected %%u after %%p");
				res = -EINVAL;
				goto error;
			} else {
				/* codecheck_ignore[LONG_LINE] */
				res = pomp_decoder_read_cbuf(dec, &v.cbuf, &len);
				if (res < 0)
					goto error;
				*va_arg(args, const void **) = v.cbuf;
				*va_arg(args, unsigned int *) = len;
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
				POMP_LOGW("decoder : unsupported format width");
				res = -EINVAL;
				goto error;
			} else if (flags & FLAG_L) {
				res = pomp_decoder_read_f64(dec, &v.f64);
				if (res < 0)
					goto error;
				*va_arg(args, double *) = v.f64;
			} else {
				res = pomp_decoder_read_f32(dec, &v.f32);
				if (res < 0)
					goto error;
				*va_arg(args, float *) = v.f32;
			}
			break;

		/* File descriptor (hack) */
		case 'x':
			if (flags & (FLAG_LL | FLAG_L | FLAG_H | FLAG_HH)) {
				POMP_LOGW("decoder : unsupported format width");
				res = -EINVAL;
				goto error;
			} else {
				res = pomp_decoder_read_fd(dec, &v.fd);
				if (res < 0)
					goto error;
				*va_arg(args, int *) = v.fd;
			}
			break;

		default:
			POMP_LOGW("decoder : invalid format specifier (%c)", c);
			res = -EINVAL;
			goto error;
		}
	}

	/* Success, caller will now need to free allocated strings */
	return 0;

	/* We need to free allocated strings in case of error */
error:
	for (i = 0; i < strsavcount; i++) {
		free(*strsav[i]);
		*strsav[i] = NULL;
	}
	return res;
}

/** Decoder dump context */
struct pomp_decoder_dump_ctx {
	char		*dst;	/**< Destination buffer */
	uint32_t	maxdst;	/**< Max length of destination */
	uint32_t	pos;	/**< Current position in destination */
	int		grow;	/**< Automatically grow destination */
};

/**
 * Append text to dump buffer.
 * @param ctx : dump context.
 * @param addlen : approximative length of new text that with be appended.
 * @param fmt : text format.
 * @param ... : format arguments.
 * @return 0 in case of success, negative errno value in case of error.
 */
POMP_ATTRIBUTE_FORMAT_PRINTF(3, 4)
static int dump_append(struct pomp_decoder_dump_ctx *ctx, uint32_t addlen,
		const char *fmt, ...)
{
	int res = 0;
	void *newdst = NULL;
	uint32_t newmaxdst = 0;
	va_list args;

	/* Resize destination buffer if needed */
	if (ctx->pos + addlen >= ctx->maxdst && ctx->grow) {
		newmaxdst = POMP_BUFFER_ALIGN_ALLOC_SIZE(ctx->pos + addlen + 1);
		newdst = realloc(ctx->dst, newmaxdst);
		if (newdst == NULL)
			return -ENOMEM;
		ctx->dst = newdst;
		ctx->maxdst = newmaxdst;
	}

	/* Stop now if destination is already full */
	if (ctx->dst == NULL || ctx->pos >= ctx->maxdst)
		return 0;

	/* Format text */
	va_start(args, fmt);
	res = vsnprintf(ctx->dst + ctx->pos, ctx->maxdst - ctx->pos, fmt, args);
	if (res >= 0) {
		if ((uint32_t)res >= ctx->maxdst - ctx->pos) {
			POMP_LOGW("decoder : dump truncated");
			ctx->pos = ctx->maxdst;
		} else {
			ctx->pos += (uint32_t)res;
		}
		res = 0;
#ifdef _WIN32
	} else {
		/* WIN32 indicates truncation with negative result ... */
		POMP_LOGW("decoder : dump truncated");
		ctx->pos = ctx->maxdst;
		res = 0;
#endif /* _WIN32 */
	}
	va_end(args);

	return res;
}

/**
 * Append buffer to dump buffer.
 * @param ctx : dump context.
 * @param buf : binary buffer to dump.
 * @param len : length of binary buffer.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int dump_append_buf(struct pomp_decoder_dump_ctx *ctx,
		const void *buf, uint32_t len)
{
	/* TODO */
	return 0;
}

/**
 * Maximum number of characters required to print a decimal value
 * U64: 18446744073709551615
 * I64: -9223372036854775808
 */
#define MAX_DEC  24

/**
 * Maximum number of characters required to print a floating point value.
 * %.7g formatting string is used
 * -2.225074e-308
 */
#define MAX_FLT  16

/**
 * Decoder dump callback.
 * @param dec : decoder.
 * @param type : type of argument.
 * @param v : value of argument.
 * @param buflen : size of buffer argument if type is POMP_PROT_DATA_TYPE_BUF.
 * @param userdata : decoder dump context;
 * @return 1 to continue dump, 0 to stop it.
 */
static int decoder_dump_cb(struct pomp_decoder *dec, uint8_t type,
		const union pomp_value *v, uint32_t buflen, void *userdata)
{
	int res = 0;
	struct pomp_decoder_dump_ctx *ctx = userdata;

	switch (type) {
	case POMP_PROT_DATA_TYPE_I8:
		res = dump_append(ctx, 5 + MAX_DEC, ", I8:%d", v->i8);
		break;

	case POMP_PROT_DATA_TYPE_U8:
		res = dump_append(ctx, 5 + MAX_DEC, ", U8:%u", v->u8);
		break;

	case POMP_PROT_DATA_TYPE_I16:
		res = dump_append(ctx, 6 + MAX_DEC, ", I16:%d", v->i16);
		break;

	case POMP_PROT_DATA_TYPE_U16:
		res = dump_append(ctx, 6 + MAX_DEC, ", U16:%u", v->u16);
		break;

	case POMP_PROT_DATA_TYPE_I32:
		res = dump_append(ctx, 6 + MAX_DEC, ", I32:%d", v->i32);
		break;

	case POMP_PROT_DATA_TYPE_U32:
		res = dump_append(ctx, 6 + MAX_DEC, ", U32:%u", v->u32);
		break;

	case POMP_PROT_DATA_TYPE_I64:
		res = dump_append(ctx, 6 + MAX_DEC, ", I64:%" PRIi64, v->i64);
		break;

	case POMP_PROT_DATA_TYPE_U64:
		res = dump_append(ctx, 6 + MAX_DEC, ", U64:%" PRIu64, v->u64);
		break;

	case POMP_PROT_DATA_TYPE_STR:
		res = dump_append(ctx, 6 + (uint32_t)strlen(v->cstr),
				", STR:'%s'", v->cstr);
		break;

	case POMP_PROT_DATA_TYPE_BUF:
		res = dump_append(ctx, 6, ", BUF:");
		if (res < 0)
			goto out;
		res = dump_append_buf(ctx, v->cbuf, buflen);
		break;

	case POMP_PROT_DATA_TYPE_F32:
		res = dump_append(ctx, 6 + MAX_FLT, ", F32:%.7g", v->f32);
		break;

	case POMP_PROT_DATA_TYPE_F64:
		res = dump_append(ctx, 6 + MAX_FLT, ", F64:%.7g", v->f64);
		break;

	case POMP_PROT_DATA_TYPE_FD:
		res = dump_append(ctx, 5 + MAX_DEC, ", FD:%d", v->fd);
		break;

	default:
		POMP_LOGW("decoder : unknown type: %d", type);
		res = -EINVAL;
		break;
	}

out:
	/* Continue if no errors and destination not full */
	return res == 0 && (ctx->pos < ctx->maxdst || ctx->grow);
}

/**
 * Decoder dump.
 * @param dec : decoder.
 * @param ctx : dump context.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int decoder_dump(struct pomp_decoder *dec,
		struct pomp_decoder_dump_ctx *ctx)
{
	int res = 0;

	/* Message id */
	res = dump_append(ctx, 4 + MAX_DEC, "{ID:%u", dec->msg->msgid);
	if (res < 0)
		goto out;

	/* Arguments */
	res = pomp_decoder_walk(dec, &decoder_dump_cb, ctx, 1);
	if (res < 0)
		goto out;

	res = dump_append(ctx, 1, "}");

out:

	/* Add ellipsis if needed */
	if (ctx->pos >= ctx->maxdst && ctx->maxdst >= 5 && !ctx->grow) {
		ctx->dst[ctx->maxdst - 5] = '.';
		ctx->dst[ctx->maxdst - 4] = '.';
		ctx->dst[ctx->maxdst - 3] = '.';
		ctx->dst[ctx->maxdst - 2] = '}';
	}

	/* Make sure it is null terminated */
	if (ctx->dst != NULL && ctx->maxdst >= 1)
		ctx->dst[ctx->maxdst - 1] = '\0';

	return res;
}

/*
 * See documentation in public header.
 */
int pomp_decoder_dump(struct pomp_decoder *dec, char *dst, uint32_t maxdst)
{
	struct pomp_decoder_dump_ctx ctx;
	POMP_RETURN_ERR_IF_FAILED(dec != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(dec->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(dec->msg->buf != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(dst != NULL, -EINVAL);

	/* Setup dump context */
	memset(&ctx, 0, sizeof(ctx));
	ctx.dst = dst;
	ctx.maxdst = maxdst;
	ctx.pos = 0;
	ctx.grow = 0;

	return decoder_dump(dec, &ctx);
}

int pomp_decoder_adump(struct pomp_decoder *dec, char **dst)
{
	int res = 0;
	struct pomp_decoder_dump_ctx ctx;
	POMP_RETURN_ERR_IF_FAILED(dec != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(dec->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(dec->msg->buf != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(dst != NULL, -EINVAL);

	*dst = NULL;

	/* Setup dump context */
	memset(&ctx, 0, sizeof(ctx));
	ctx.dst = NULL;
	ctx.maxdst = 0;
	ctx.pos = 0;
	ctx.grow = 1;

	res = decoder_dump(dec, &ctx);

	if (res < 0)
		free(ctx.dst);
	else
		*dst = ctx.dst;
	return res;
}

/**
 * Walk the internal buffer and call given callback for each argument found.
 * @param dec : decoder.
 * @param cb : function to call for each argument. The return value shall be 1
 * to continue the operation, 0 to stop it.
 * @param userdata : user data for callback.
 * @param checkfds : 1 to check that file descriptors are correctly regsitered
 * in buffer, 0 to simply read an 'int'. Used by the fixup function just after
 * the message has been received.
 * @return 0 in case of success, negative errno value in case of error.
 */
int pomp_decoder_walk(struct pomp_decoder *dec,
		pomp_decoder_walk_cb_t cb, void *userdata, int checkfds)
{
	int res = 0;
	uint8_t type = 0;
	uint32_t buflen = 0;
	uint8_t skipped[sizeof(uint8_t) + sizeof(int32_t)];
	union pomp_value v;

	POMP_RETURN_ERR_IF_FAILED(dec != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(dec->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(dec->msg->buf != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(cb != NULL, -EINVAL);

	/* Process message arguments */
	while (res == 0 && dec->pos < dec->msg->buf->len) {
		/* Read type */
		res = pomp_buffer_readb(dec->msg->buf, &dec->pos, &type);
		if (res < 0)
			goto out;

		/* Rewind for further decoding */
		dec->pos -= sizeof(uint8_t);
		memset(&v, 0, sizeof(v));
		buflen = 0;
		switch (type) {
		case POMP_PROT_DATA_TYPE_I8:
			res = pomp_decoder_read_i8(dec, &v.i8);
			break;

		case POMP_PROT_DATA_TYPE_U8:
			res = pomp_decoder_read_u8(dec, &v.u8);
			break;

		case POMP_PROT_DATA_TYPE_I16:
			res = pomp_decoder_read_i16(dec, &v.i16);
			break;

		case POMP_PROT_DATA_TYPE_U16:
			res = pomp_decoder_read_u16(dec, &v.u16);
			break;

		case POMP_PROT_DATA_TYPE_I32:
			res = pomp_decoder_read_i32(dec, &v.i32);
			break;

		case POMP_PROT_DATA_TYPE_U32:
			res = pomp_decoder_read_u32(dec, &v.u32);
			break;

		case POMP_PROT_DATA_TYPE_I64:
			res = pomp_decoder_read_i64(dec, &v.i64);
			break;

		case POMP_PROT_DATA_TYPE_U64:
			res = pomp_decoder_read_u64(dec, &v.u64);
			break;

		case POMP_PROT_DATA_TYPE_STR:
			res = pomp_decoder_read_cstr(dec, &v.cstr);
			break;

		case POMP_PROT_DATA_TYPE_BUF:
			res = pomp_decoder_read_cbuf(dec, &v.cbuf, &buflen);
			break;

		case POMP_PROT_DATA_TYPE_F32:
			res = pomp_decoder_read_f32(dec, &v.f32);
			break;

		case POMP_PROT_DATA_TYPE_F64:
			res = pomp_decoder_read_f64(dec, &v.f64);
			break;

		case POMP_PROT_DATA_TYPE_FD:
			if (checkfds) {
				res = pomp_decoder_read_fd(dec, &v.fd);
			} else {
				/* Skip type and data */
				res = pomp_buffer_read(dec->msg->buf,
						&dec->pos, skipped,
						sizeof(skipped));
				v.fd = -1;
			}
			break;

		default:
			POMP_LOGW("decoder : unknown type: %d", type);
			res = -EINVAL;
			break;
		}

		if (res < 0)
			goto out;

		/* Call user callback, stop it it returns 0 */
		if ((*cb)(dec, type, &v, buflen, userdata) == 0)
			goto out;
	}

out:
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_decoder_read_i8(struct pomp_decoder *dec, int8_t *v)
{
	int res = 0;
	uint8_t d = 0;
	POMP_RETURN_ERR_IF_FAILED(dec != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(dec->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(v != NULL, -EINVAL);
	res = decoder_read_data(dec, POMP_PROT_DATA_TYPE_I8, &d, sizeof(d));
	*v = (int8_t)d;
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_decoder_read_u8(struct pomp_decoder *dec, uint8_t *v)
{
	int res = 0;
	uint8_t d = 0;
	POMP_RETURN_ERR_IF_FAILED(dec != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(dec->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(v != NULL, -EINVAL);
	res = decoder_read_data(dec, POMP_PROT_DATA_TYPE_U8, &d, sizeof(d));
	*v = d;
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_decoder_read_i16(struct pomp_decoder *dec, int16_t *v)
{
	int res = 0;
	uint16_t d = 0;
	POMP_RETURN_ERR_IF_FAILED(dec != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(dec->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(v != NULL, -EINVAL);
	res = decoder_read_data(dec, POMP_PROT_DATA_TYPE_I16, &d, sizeof(d));
	*v = (int16_t)POMP_LE16TOH(d);
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_decoder_read_u16(struct pomp_decoder *dec, uint16_t *v)
{
	int res = 0;
	uint16_t d = 0;
	POMP_RETURN_ERR_IF_FAILED(dec != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(dec->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(v != NULL, -EINVAL);
	res = decoder_read_data(dec, POMP_PROT_DATA_TYPE_U16, &d, sizeof(d));
	*v = POMP_LE16TOH(d);
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_decoder_read_i32(struct pomp_decoder *dec, int32_t *v)
{
	int res = 0;
	uint64_t d = 0;
	POMP_RETURN_ERR_IF_FAILED(dec != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(dec->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(v != NULL, -EINVAL);
	res = decoder_read_varint(dec, POMP_PROT_DATA_TYPE_I32, &d);
	/* Zigzag decoding, use logical right shift, without sign propagation */
	*v = ((int32_t)(d >> 1)) ^ -((int32_t)(d & 0x1));
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_decoder_read_u32(struct pomp_decoder *dec, uint32_t *v)
{
	int res = 0;
	uint64_t d = 0;
	POMP_RETURN_ERR_IF_FAILED(dec != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(dec->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(v != NULL, -EINVAL);
	res = decoder_read_varint(dec, POMP_PROT_DATA_TYPE_U32, &d);
	*v = (uint32_t)d;
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_decoder_read_i64(struct pomp_decoder *dec, int64_t *v)
{
	int res = 0;
	uint64_t d = 0;
	POMP_RETURN_ERR_IF_FAILED(dec != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(dec->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(v != NULL, -EINVAL);
	res = decoder_read_varint(dec, POMP_PROT_DATA_TYPE_I64, &d);
	/* Zigzag decoding, use logical right shift, without sign propagation */
	*v = ((int64_t)(d >> 1)) ^ -((int64_t)(d & 0x1));
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_decoder_read_u64(struct pomp_decoder *dec, uint64_t *v)
{
	int res = 0;
	uint64_t d = 0;
	POMP_RETURN_ERR_IF_FAILED(dec != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(dec->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(v != NULL, -EINVAL);
	res = decoder_read_varint(dec, POMP_PROT_DATA_TYPE_U64, &d);
	*v = d;
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_decoder_read_str(struct pomp_decoder *dec, char **v)
{
	int res = 0;
	const char *cstr = NULL;

	POMP_RETURN_ERR_IF_FAILED(dec != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(dec->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(v != NULL, -EINVAL);

	/* Get string without copy */
	res = pomp_decoder_read_cstr(dec, &cstr);
	if (res < 0)
		return res;

	/* Now copy it */
	*v = strdup(cstr);
	if (*v == NULL)
		return -ENOMEM;

	/* Success */
	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_decoder_read_cstr(struct pomp_decoder *dec, const char **v)
{
	int res = 0;
	uint8_t readtype = 0;
	const void *pstr = NULL;
	const char *cstr = NULL;
	uint16_t len = 0;

	POMP_RETURN_ERR_IF_FAILED(dec != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(dec->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(v != NULL, -EINVAL);

	/* Read type */
	res = pomp_buffer_readb(dec->msg->buf, &dec->pos, &readtype);
	if (res < 0)
		return res;

	/* Check type, rewind in case of mismatch */
	if (readtype != POMP_PROT_DATA_TYPE_STR) {
		POMP_LOGW("decoder : type mismatch %d(%d)",
				readtype, POMP_PROT_DATA_TYPE_STR);
		dec->pos -= sizeof(uint8_t);
		return -EINVAL;
	}

	/* Read size */
	res = decoder_read_size_u16(dec, &len);
	if (res < 0)
		return res;
	if (len == 0) {
		POMP_LOGW("decoder : invalid string length (%u)", len);
		return -EINVAL;
	}

	/* Get string from buffer (no copy done) */
	res = pomp_buffer_cread(dec->msg->buf, &dec->pos, &pstr, len);
	if (res < 0)
		return res;

	/* Check that string is null terminated */
	cstr = (const char *)pstr;
	if (cstr[len - 1] != '\0') {
		POMP_LOGW("decoder : string not null terminated");
		return -EINVAL;
	}

	/* Success */
	*v = cstr;
	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_decoder_read_buf(struct pomp_decoder *dec, void **v, uint32_t *n)
{
	int res = 0;
	const void *p = NULL;

	POMP_RETURN_ERR_IF_FAILED(dec != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(dec->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(v != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(n != NULL, -EINVAL);

	/* Get data without copy */
	res = pomp_decoder_read_cbuf(dec, &p, n);
	if (res < 0)
		return res;

	/* Now copy it */
	*v = malloc(*n);
	if (*v == NULL)
		return -ENOMEM;
	memcpy(*v, p, *n);

	/* Success */
	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_decoder_read_cbuf(struct pomp_decoder *dec, const void **v,
		uint32_t *n)
{
	int res = 0;
	uint8_t readtype = 0;
	const void *p = NULL;
	uint32_t len = 0;

	POMP_RETURN_ERR_IF_FAILED(dec != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(dec->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(v != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(n != NULL, -EINVAL);

	/* Read type */
	res = pomp_buffer_readb(dec->msg->buf, &dec->pos, &readtype);
	if (res < 0)
		return res;

	/* Check type, rewind in case of mismatch */
	if (readtype != POMP_PROT_DATA_TYPE_BUF) {
		POMP_LOGW("decoder : type mismatch %d(%d)",
				readtype, POMP_PROT_DATA_TYPE_BUF);
		dec->pos -= sizeof(uint8_t);
		return -EINVAL;
	}

	/* Read size */
	res = decoder_read_size_u32(dec, &len);
	if (res < 0)
		return res;

	/* Get data from buffer (no copy done) */
	res = pomp_buffer_cread(dec->msg->buf, &dec->pos, &p, len);
	if (res < 0)
		return res;

	/* Success */
	*v = p;
	*n = len;
	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_decoder_read_f32(struct pomp_decoder *dec, float *v)
{
	int res = 0;
	union {
		float f32;
		uint32_t u32;
	} d;
	POMP_RETURN_ERR_IF_FAILED(dec != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(dec->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(v != NULL, -EINVAL);
	res = decoder_read_data(dec, POMP_PROT_DATA_TYPE_F32, &d, sizeof(d));
	d.u32 = POMP_LE32TOH(d.u32);
	*v = d.f32;
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_decoder_read_f64(struct pomp_decoder *dec, double *v)
{
	int res = 0;
	union {
		double f64;
		uint64_t u64;
	} d;
	POMP_RETURN_ERR_IF_FAILED(dec != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(dec->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(v != NULL, -EINVAL);
	res = decoder_read_data(dec, POMP_PROT_DATA_TYPE_F64, &d, sizeof(d));
	d.u64 = POMP_LE64TOH(d.u64);
	*v = d.f64;
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_decoder_read_fd(struct pomp_decoder *dec, int *v)
{
	int res = 0;
	uint8_t readtype = 0;
	POMP_RETURN_ERR_IF_FAILED(dec != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(dec->msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(v != NULL, -EINVAL);

	/* Read type */
	res = pomp_buffer_readb(dec->msg->buf, &dec->pos, &readtype);
	if (res < 0)
		return res;

	/* Check type, rewind in case of mismatch */
	if (readtype != POMP_PROT_DATA_TYPE_FD) {
		POMP_LOGW("decoder : type mismatch %d(%d)",
				readtype, POMP_PROT_DATA_TYPE_FD);
		dec->pos -= sizeof(uint8_t);
		return -EINVAL;
	}

	/* Read file descriptor */
	*v = -1;
	return pomp_buffer_read_fd(dec->msg->buf, &dec->pos, v);
}
