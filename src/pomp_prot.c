/**
 * @file pomp_prot.c
 *
 * @brief Handle message protocol parsing.
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

/** Protocol header */
struct pomp_prot_header {
	uint8_t		magic[4];	/**< Magic */
	uint32_t	msgid;		/**< Message id */
	uint32_t	size;		/**< Size of message (with header) */
};

/** Protocol decoding state */
enum pomp_prot_state {
	POMP_PROT_STATE_IDLE = 0,	/**< Idle */
	POMP_PROT_STATE_HEADER_MAGIC_0,	/**< Waiting for magic 0 */
	POMP_PROT_STATE_HEADER_MAGIC_1,	/**< Waiting for magic 1 */
	POMP_PROT_STATE_HEADER_MAGIC_2,	/**< Waiting for magic 2 */
	POMP_PROT_STATE_HEADER_MAGIC_3,	/**< Waiting for magic 3 */
	POMP_PROT_STATE_HEADER,		/**< Reading header */
	POMP_PROT_STATE_PAYLOAD,	/**< Reading payload */
};

/** Protocol structure */
struct pomp_prot {
	/** Decoding state */
	enum pomp_prot_state	state;
	/** Buffer to read message header */
	uint8_t			headerbuf[POMP_PROT_HEADER_SIZE];
	/** Decoded message header */
	struct pomp_prot_header	header;
	/** Current offset in header decoding */
	size_t			offheader;
	/** Current offset in payload decoding */
	size_t			offpayload;
	/** Associated message */
	struct pomp_msg		*msg;
};

/**
 * Reset the internal state of the protocol decoder.
 * @param prot : protocol decoder.
 */
static void pomp_prot_reset_state(struct pomp_prot *prot)
{
	prot->state = POMP_PROT_STATE_IDLE;
	memset(&prot->headerbuf, 0, sizeof(prot->headerbuf));
	memset(&prot->header, 0, sizeof(prot->header));
	prot->offheader = 0;
	prot->offpayload = 0;
}

/**
 * Make sure the internal message object is properly allocated.
 * @param prot : protocol decoder.
 * @param msgid : message id to set.
 * @param size : total size of message to allocate.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int pomp_prot_alloc_msg(struct pomp_prot *prot, uint32_t msgid,
		size_t size)
{
	int res = 0;

	/* Allocate new message if needed */
	if (prot->msg == NULL)
		prot->msg = pomp_msg_new();
	if (prot->msg == NULL)
		return -ENOMEM;

	/* Initialize message, setup buffer inside message */
	res = pomp_msg_init(prot->msg, msgid);
	if (res < 0)
		return res;
	return pomp_buffer_ensure_capacity(prot->msg->buf, size);
}

/**
 * Check that the magic bytes of the message header are valid.
 * @param prot : protocol decoder.
 * @param idx : index of magic byte to check.
 * @param val : expected value of magic byte.
 * @param state : next state if check is ok.
 */
static void pomp_prot_check_magic(struct pomp_prot *prot, int idx, int val,
		enum pomp_prot_state state)
{
	if (prot->headerbuf[idx] != val) {
		POMP_LOGW("Bad header magic %d : 0x%02x(0x%02x)",
			idx, prot->headerbuf[idx], val);
		prot->state = POMP_PROT_STATE_HEADER_MAGIC_0;
	} else {
		prot->state = state;
	}
}

/**
 * Decode the header of the message.
 * @param prot : protocol decoder.
 *
 * @remarks in case of error during header decoding or message allocation,
 * the decoder state is reseted.
 */
static void pomp_prot_decode_header(struct pomp_prot *prot)
{
	uint32_t d = 0;

	/* Decode header inside structure */
	memcpy(&prot->header.magic[0], prot->headerbuf, 4);
	memcpy(&d, &prot->headerbuf[4], 4);
	prot->header.msgid = POMP_LE32TOH(d);
	memcpy(&d, &prot->headerbuf[8], 4);
	prot->header.size = POMP_LE32TOH(d);

	/* Check header and setup payload decoding */
	if (prot->header.size < POMP_PROT_HEADER_SIZE) {
		POMP_LOGW("Bad header size : %d", prot->header.size);
		prot->state = POMP_PROT_STATE_HEADER_MAGIC_0;
	} else if (pomp_prot_alloc_msg(prot, prot->header.msgid,
			prot->header.size) < 0) {
		prot->state = POMP_PROT_STATE_HEADER_MAGIC_0;
	} else {
		/* Copy header in message buffer */
		prot->offpayload = 0;
		pomp_buffer_write(prot->msg->buf, &prot->offpayload,
				prot->headerbuf, POMP_PROT_HEADER_SIZE);
		prot->state = POMP_PROT_STATE_PAYLOAD;
	}
}

/**
 * Copy data. Up to lensrc - *offsrc bytes will be written.
 * @param basedst : base address of destination.
 * @param offdst : offset of destination, updated after the copy.
 * @param lendst : total size of destination.
 * @param basesrc : base address of source.
 * @param offsrc : offset of source, updated after the copy.
 * @param lensrc : total size of source.
 */
static void copy(void *basedst, size_t *offdst, size_t lendst,
		const void *basesrc, size_t *offsrc, size_t lensrc)
{
	/* Compute destination and source */
	void *dst = ((uint8_t *)(basedst)) + *offdst;
	const void *src = ((const uint8_t *)(basesrc)) + *offsrc;

	/* Determine copy length */
	size_t lencpy = lensrc - *offsrc;
	if (lencpy > lendst - *offdst)
		lencpy = lendst - *offdst;
	if (lencpy == 0)
		return;

	/* Do the copy and update offsets */
	memcpy(dst, src, lencpy);
	*offdst += lencpy;
	*offsrc += lencpy;
}

/**
 * Copy magic byte in header. Only one byte will be written.
 * @param prot : protocol decoder.
 * @param basesrc : base address of source.
 * @param offsrc : offset of source, updated after the copy.
 * @param lensrc : total size of source.
 */
static void copy_header_magic(struct pomp_prot *prot,
		const void *basesrc, size_t *offsrc, size_t lensrc)
{
	copy(prot->headerbuf, &prot->offheader, POMP_PROT_HEADER_SIZE,
			basesrc, offsrc, *offsrc + 1);
}

/**
 * Copy header bytes. Up to lensrc - *offsrc bytes will be written.
 * @param prot : protocol decoder.
 * @param basesrc : base address of source.
 * @param offsrc : offset of source, updated after the copy.
 * @param lensrc : total size of source.
 */
static void copy_header(struct pomp_prot *prot,
		const void *basesrc, size_t *offsrc, size_t lensrc)
{
	copy(prot->headerbuf, &prot->offheader, POMP_PROT_HEADER_SIZE,
			basesrc, offsrc, lensrc);
}

/**
 * Copy payload bytes. Up to lensrc - *offsrc bytes will be written.
 * @param prot : protocol decoder.
 * @param basesrc : base address of source.
 * @param offsrc : offset of source, updated after the copy.
 * @param lensrc : total size of source.
 */
static void copy_payload(struct pomp_prot *prot,
		const void *basesrc, size_t *offsrc, size_t lensrc)
{
	const void *src = ((const uint8_t *)(basesrc)) + *offsrc;

	/* Determine copy length */
	size_t lencpy = lensrc - *offsrc;
	if (lencpy > prot->header.size - prot->offpayload)
		lencpy = prot->header.size - prot->offpayload;
	if (lencpy == 0)
		return;

	/* Should not fail as the buffer has already been pre-allocated */
	pomp_buffer_write(prot->msg->buf, &prot->offpayload, src, lencpy);
	*offsrc += lencpy;
}

/**
 * Create a new protocol decoder object.
 * @return protocol decoder object or NULL in case of error.
 */
struct pomp_prot *pomp_prot_new(void)
{
	struct pomp_prot *prot = NULL;

	/* Allocate prot structure */
	prot = calloc(1, sizeof(*prot));
	if (prot == NULL)
		return NULL;

	/* Reset internal state */
	pomp_prot_reset_state(prot);
	return prot;
}

/**
 * Destroy a protocol decoder object.
 * @param prot : protocol decoder.
 * @return 0 in case of success, negative errno value in case of error.
 */
int pomp_prot_destroy(struct pomp_prot *prot)
{
	POMP_RETURN_ERR_IF_FAILED(prot != NULL, -EINVAL);
	if (prot->msg != NULL)
		pomp_msg_destroy(prot->msg);
	pomp_prot_reset_state(prot);
	free(prot);
	return 0;
}

/**
 * Try to decode a message with given input data.
 * @param prot : protocol decoder.
 * @param buf : input data.
 * @param len : size of input data.
 * @param msg : will receive decoded message. If more input data is required
 * to decode the message, NULL will be returned. The message needs to be
 * released either by calling 'pomp_msg_destroy' or 'pomp_prot_release_msg'.
 * Calling 'pomp_prot_release_msg' allows reuse of allocated message structure.
 * @return number of bytes processed. It can be less that input size in which
 * case caller shall call again this function with remaining bytes.
 */
ssize_t pomp_prot_decode_msg(struct pomp_prot *prot, const void *buf,
		size_t len, struct pomp_msg **msg)
{
	size_t off = 0;

	POMP_RETURN_ERR_IF_FAILED(prot != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(buf != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(msg != NULL, -EINVAL);

	/* If idle, start a new parsing */
	if (prot->state == POMP_PROT_STATE_IDLE)
		prot->state = POMP_PROT_STATE_HEADER_MAGIC_0;

	/* Processing loop */
	while (off < len && prot->state != POMP_PROT_STATE_IDLE) {
		switch (prot->state) {
		case POMP_PROT_STATE_IDLE: /* NO BREAK */
		case POMP_PROT_STATE_HEADER_MAGIC_0:
			pomp_prot_reset_state(prot);
			prot->state = POMP_PROT_STATE_HEADER_MAGIC_0;
			copy_header_magic(prot, buf, &off, len);
			pomp_prot_check_magic(prot, 0, POMP_PROT_HEADER_MAGIC_0,
					POMP_PROT_STATE_HEADER_MAGIC_1);
			break;

		case POMP_PROT_STATE_HEADER_MAGIC_1:
			copy_header_magic(prot, buf, &off, len);
			pomp_prot_check_magic(prot, 1, POMP_PROT_HEADER_MAGIC_1,
					POMP_PROT_STATE_HEADER_MAGIC_2);
			break;

		case POMP_PROT_STATE_HEADER_MAGIC_2:
			copy_header_magic(prot, buf, &off, len);
			pomp_prot_check_magic(prot, 2, POMP_PROT_HEADER_MAGIC_2,
					POMP_PROT_STATE_HEADER_MAGIC_3);
			break;

		case POMP_PROT_STATE_HEADER_MAGIC_3:
			copy_header_magic(prot, buf, &off, len);
			pomp_prot_check_magic(prot, 3, POMP_PROT_HEADER_MAGIC_3,
					POMP_PROT_STATE_HEADER);
			break;

		case POMP_PROT_STATE_HEADER:
			copy_header(prot, buf, &off, len);
			if (prot->offheader == POMP_PROT_HEADER_SIZE)
				pomp_prot_decode_header(prot);
			break;

		case POMP_PROT_STATE_PAYLOAD:
			copy_payload(prot, buf, &off, len);
			break;

		default:
			POMP_LOGE("Invalid state %d", prot->state);
			break;
		}

		/* Check end of payload */
		if (prot->state == POMP_PROT_STATE_PAYLOAD
				&& prot->offpayload == prot->header.size) {
			/* Give ownership of message to caller */
			prot->msg->finished = 1;
			*msg = prot->msg;
			prot->msg = NULL;
			prot->state = POMP_PROT_STATE_IDLE;
		}
	}

	/* Return number of bytes processed */
	return (ssize_t)off;
}

/**
 * Release a previously decoded message. This is to reuse message structure
 * if possible and avoid some malloc/free at each decoded message. If there
 * is already a internal message structure, it is simply destroyed.
 * @param prot : protocol decoder.
 * @param msg : message to release.
 * @return 0 in case of success, negative errno value in case of error.
 */
int pomp_prot_release_msg(struct pomp_prot *prot, struct pomp_msg *msg)
{
	POMP_RETURN_ERR_IF_FAILED(prot != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(msg != NULL, -EINVAL);

	/* if we already have one, destroy given one, otherwise get ownership
	 * but clear it */
	if (prot->msg != NULL) {
		pomp_msg_destroy(msg);
	} else {
		prot->msg = msg;
		pomp_msg_clear(prot->msg);
	}
	return 0;
}
