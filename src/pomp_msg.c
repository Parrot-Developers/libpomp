/**
 * @file pomp_msg.c
 *
 * @brief Handle formating/decoding of messages
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

/*
 * See documentation in public header.
 */
struct pomp_msg *pomp_msg_new(void)
{
	struct pomp_msg *msg = NULL;

	/* Allocate message structure */
	msg = calloc(1, sizeof(*msg));
	if (msg == NULL)
		return NULL;
	return msg;
}

/*
 * See documentation in public header.
 */
struct pomp_msg *pomp_msg_new_copy(const struct pomp_msg *msg)
{
	struct pomp_msg *newmsg = NULL;

	POMP_RETURN_VAL_IF_FAILED(msg != NULL, -EINVAL, NULL);

	/* Allocate message structure */
	newmsg = calloc(1, sizeof(*newmsg));
	if (newmsg == NULL)
		goto error;

	newmsg->msgid = msg->msgid;
	newmsg->finished = msg->finished;

	/* Copy buffer */
	if (msg->buf != NULL) {
		newmsg->buf = pomp_buffer_new_copy(msg->buf);
		if (newmsg->buf == NULL)
			goto error;
	}

	return newmsg;

	/* Cleanup in case of error */
error:
	if (newmsg != NULL) {
		if (newmsg->buf != NULL)
			pomp_buffer_unref(newmsg->buf);
		free(newmsg);
	}
	return NULL;
}

/*
 * See documentation in public header.
 */
struct pomp_msg *pomp_msg_new_with_buffer(struct pomp_buffer *buf)
{
	struct pomp_msg *msg = NULL;
	size_t pos = 0;
	uint32_t d = 0;

	POMP_RETURN_VAL_IF_FAILED(buf != NULL, -EINVAL, NULL);

	/* Allocate message structure */
	msg = calloc(1, sizeof(*msg));
	if (msg == NULL)
		goto error;

	/* Get a new ref on buffer */
	msg->finished = 1;
	msg->buf = buf;
	pomp_buffer_ref(buf);

	/* Make sure header is valid */
	if (msg->buf->len < POMP_PROT_HEADER_SIZE) {
		POMP_LOGW("Bad header size: %u", (uint32_t)msg->buf->len);
		goto error;
	}

	/* Check magic */
	(void)pomp_buffer_read(msg->buf, &pos, &d, sizeof(d));
	if (POMP_LE32TOH(d) != POMP_PROT_HEADER_MAGIC) {
		POMP_LOGW("Bad header magic: %08x(%08x)",
				POMP_LE32TOH(d), POMP_PROT_HEADER_MAGIC);
		goto error;
	}

	/* Message id */
	(void)pomp_buffer_read(msg->buf, &pos, &d, sizeof(d));
	msg->msgid = POMP_LE32TOH(d);

	/* Check message size */
	(void)pomp_buffer_read(msg->buf, &pos, &d, sizeof(d));
	if (POMP_LE32TOH(d) != buf->len) {
		POMP_LOGW("Bad message size: %08x(%08x)",
				(uint32_t)buf->len, POMP_LE32TOH(d));
		goto error;
	}

	return msg;

	/* Cleanup in case of error */
error:
	if (msg != NULL) {
		if (msg->buf != NULL)
			pomp_buffer_unref(msg->buf);
		free(msg);
	}
	return NULL;
}

/*
 * See documentation in public header.
 */
int pomp_msg_destroy(struct pomp_msg *msg)
{
	POMP_RETURN_ERR_IF_FAILED(msg != NULL, -EINVAL);
	(void)pomp_msg_clear(msg);
	free(msg);
	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_msg_init(struct pomp_msg *msg, uint32_t msgid)
{
	POMP_RETURN_ERR_IF_FAILED(msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(msg->buf == NULL, -EPERM);

	msg->msgid = msgid;
	msg->finished = 0;

	/* Allocate new buffer */
	msg->buf = pomp_buffer_new(0);
	if (msg->buf == NULL)
		return -ENOMEM;

	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_msg_finish(struct pomp_msg *msg)
{
	int res = 0;
	size_t pos = 0;
	uint32_t d = 0;

	POMP_RETURN_ERR_IF_FAILED(msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(msg->buf != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(!msg->finished, -EINVAL);

	/* Make sure we will be able to write header */
	res = pomp_buffer_ensure_capacity(msg->buf, POMP_PROT_HEADER_SIZE);
	if (res < 0)
		return res;

	/* Below writes should not fail because allocation is ok */

	/* Magic */
	(void)pomp_buffer_writeb(msg->buf, &pos, POMP_PROT_HEADER_MAGIC_0);
	(void)pomp_buffer_writeb(msg->buf, &pos, POMP_PROT_HEADER_MAGIC_1);
	(void)pomp_buffer_writeb(msg->buf, &pos, POMP_PROT_HEADER_MAGIC_2);
	(void)pomp_buffer_writeb(msg->buf, &pos, POMP_PROT_HEADER_MAGIC_3);

	/* Message id */
	d = POMP_HTOLE32(msg->msgid);
	(void)pomp_buffer_write(msg->buf, &pos, &d, sizeof(d));

	/* Message size (make sure we have at least the header size in
	 * case no payload was written in buffer) */
	if (msg->buf->len < POMP_PROT_HEADER_SIZE)
		d = POMP_HTOLE32(POMP_PROT_HEADER_SIZE);
	else
		d = POMP_HTOLE32((uint32_t)msg->buf->len);
	(void)pomp_buffer_write(msg->buf, &pos, &d, sizeof(d));

	/* Message can not be modified anymore */
	msg->finished = 1;
	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_msg_clear(struct pomp_msg *msg)
{
	POMP_RETURN_ERR_IF_FAILED(msg != NULL, -EINVAL);

	msg->msgid = 0;
	msg->finished = 0;

	/* Release buffer */
	if (msg->buf != NULL)
		pomp_buffer_unref(msg->buf);
	msg->buf = NULL;

	return 0;
}

/*
 * See documentation in public header.
 */
uint32_t pomp_msg_get_id(const struct pomp_msg *msg)
{
	POMP_RETURN_VAL_IF_FAILED(msg != NULL, -EINVAL, 0);
	return msg->msgid;
}

/*
 * See documentation in public header.
 */
struct pomp_buffer *pomp_msg_get_buffer(const struct pomp_msg *msg)
{
	POMP_RETURN_VAL_IF_FAILED(msg != NULL, -EINVAL, NULL);
	return msg->buf;
}

/*
 * See documentation in public header.
 */
int pomp_msg_write(struct pomp_msg *msg, uint32_t msgid, const char *fmt, ...)
{
	int res = 0;
	va_list args;
	va_start(args, fmt);
	res = pomp_msg_writev(msg, msgid, fmt, args);
	va_end(args);
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_msg_writev(struct pomp_msg *msg, uint32_t msgid, const char *fmt,
		va_list args)
{
	int res = 0;
	struct pomp_encoder enc = POMP_ENCODER_INITIALIZER;

	POMP_RETURN_ERR_IF_FAILED(msg != NULL, -EINVAL);

	/* Initialize message */
	res = pomp_msg_init(msg, msgid);
	if (res < 0)
		goto out;

	/* Setup encoder */
	res = pomp_encoder_init(&enc, msg);
	if (res < 0)
		goto out;

	/* Encode message */
	res = pomp_encoder_writev(&enc, fmt, args);
	if (res < 0)
		goto out;

	/* Finish it */
	res = pomp_msg_finish(msg);
	if (res < 0)
		goto out;

out:
	/* Cleanup */
	(void)pomp_encoder_clear(&enc);
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_msg_write_argv(struct pomp_msg *msg, uint32_t msgid,
		const char *fmt, int argc, const char * const *argv)
{
	int res = 0;
	struct pomp_encoder enc = POMP_ENCODER_INITIALIZER;

	POMP_RETURN_ERR_IF_FAILED(msg != NULL, -EINVAL);

	/* Initialize message */
	res = pomp_msg_init(msg, msgid);
	if (res < 0)
		goto out;

	/* Setup encoder */
	res = pomp_encoder_init(&enc, msg);
	if (res < 0)
		goto out;

	/* Encode message */
	res = pomp_encoder_write_argv(&enc, fmt, argc, argv);
	if (res < 0)
		goto out;

	/* Finish it */
	res = pomp_msg_finish(msg);
	if (res < 0)
		goto out;

out:
	/* Cleanup */
	(void)pomp_encoder_clear(&enc);
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_msg_read(const struct pomp_msg *msg, const char *fmt, ...)
{
	int res = 0;
	va_list args;
	va_start(args, fmt);
	res = pomp_msg_readv(msg, fmt, args);
	va_end(args);
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_msg_readv(const struct pomp_msg *msg, const char *fmt, va_list args)
{
	int res = 0;
	struct pomp_decoder dec = POMP_DECODER_INITIALIZER;

	POMP_RETURN_ERR_IF_FAILED(msg != NULL, -EINVAL);

	res = pomp_decoder_init(&dec, msg);
	if (res == 0)
		res = pomp_decoder_readv(&dec, fmt, args);

	/* Always clear decoder, even in case of error during decoding */
	(void)pomp_decoder_clear(&dec);
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_msg_dump(const struct pomp_msg *msg,
		char *dst, uint32_t maxdst)
{
	int res = 0;
	struct pomp_decoder dec = POMP_DECODER_INITIALIZER;

	POMP_RETURN_ERR_IF_FAILED(msg != NULL, -EINVAL);

	res = pomp_decoder_init(&dec, msg);
	if (res == 0)
		res = pomp_decoder_dump(&dec, dst, maxdst);

	/* Always clear decoder, even in case of error during dump */
	(void)pomp_decoder_clear(&dec);
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_msg_adump(const struct pomp_msg *msg, char **dst)
{
	int res = 0;
	struct pomp_decoder dec = POMP_DECODER_INITIALIZER;

	POMP_RETURN_ERR_IF_FAILED(msg != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(dst != NULL, -EINVAL);
	*dst = NULL;

	res = pomp_decoder_init(&dec, msg);
	if (res == 0)
		res = pomp_decoder_adump(&dec, dst);

	/* Always clear decoder, even in case of error during dump */
	(void)pomp_decoder_clear(&dec);
	return res;
}
