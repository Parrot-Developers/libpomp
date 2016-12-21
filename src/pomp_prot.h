/**
 * @file pomp_prot.h
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

#ifndef _POMP_PROT_H_
#define _POMP_PROT_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Endianess detection */
#if !defined(POMP_LITTLE_ENDIAN) && !defined(POMP_BIG_ENDIAN)
#  if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#    define POMP_LITTLE_ENDIAN
#  elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#    define POMP_BIG_ENDIAN
#  elif defined(_WIN32)
#    define POMP_LITTLE_ENDIAN
#  else
#    ifdef __APPLE__
#      include <machine/endian.h>
#    else
#      include <endian.h>
#    endif
#    if defined(BYTE_ORDER) && (BYTE_ORDER == LITTLE_ENDIAN)
#      define POMP_LITTLE_ENDIAN
#    elif defined(BYTE_ORDER) && (BYTE_ORDER == BIG_ENDIAN)
#      define POMP_BIG_ENDIAN
#    else
#      error Unable to determine endianess of machine
#    endif
#  endif
#endif

#ifdef POMP_LITTLE_ENDIAN

/** Convert 16-bit integer from host ordering to little endian */
#define POMP_HTOLE16(_x)	((uint16_t)(_x))

/** Convert 32-bit integer from host ordering to little endian */
#define POMP_HTOLE32(_x)	((uint32_t)(_x))

/** Convert 64-bit integer from host ordering to little endian */
#define POMP_HTOLE64(_x)	((uint64_t)(_x))

/** Convert 16-bit integer from little endian to host ordering */
#define POMP_LE16TOH(_x)	((uint16_t)(_x))

/** Convert 32-bit integer from little endian to host ordering */
#define POMP_LE32TOH(_x)	((uint32_t)(_x))

/** Convert 64-bit integer from little endian to host ordering */
#define POMP_LE64TOH(_x)	((uint64_t)(_x))

#else

#error Big endian machines not yet supported

#endif

/* Magic bytes */
#define POMP_PROT_HEADER_MAGIC_0	'P'	/**< Magic byte 0 */
#define POMP_PROT_HEADER_MAGIC_1	'O'	/**< Magic byte 1 */
#define POMP_PROT_HEADER_MAGIC_2	'M'	/**< Magic byte 2 */
#define POMP_PROT_HEADER_MAGIC_3	'P'	/**< Magic byte 3 */

/** 32-bit magic */
#define POMP_PROT_HEADER_MAGIC \
	(POMP_PROT_HEADER_MAGIC_0 | \
	(POMP_PROT_HEADER_MAGIC_1 << 8) | \
	(POMP_PROT_HEADER_MAGIC_2 << 16) | \
	(POMP_PROT_HEADER_MAGIC_3 << 24))

/* Data types */
#define POMP_PROT_DATA_TYPE_I8		0x01	/**< 8-bit signed integer */
#define POMP_PROT_DATA_TYPE_U8		0x02	/**< 8-bit unsigned integer */
#define POMP_PROT_DATA_TYPE_I16		0x03	/**< 16-bit signed integer */
#define POMP_PROT_DATA_TYPE_U16		0x04	/**< 16-bit unsigned integer */
#define POMP_PROT_DATA_TYPE_I32		0x05	/**< 32-bit signed integer */
#define POMP_PROT_DATA_TYPE_U32		0x06	/**< 32-bit unsigned integer */
#define POMP_PROT_DATA_TYPE_I64		0x07	/**< 64-bit signed integer */
#define POMP_PROT_DATA_TYPE_U64		0x08	/**< 64-bit unsigned integer */
#define POMP_PROT_DATA_TYPE_STR		0x09	/**< String */
#define POMP_PROT_DATA_TYPE_BUF		0x0a	/**< Buffer */
#define POMP_PROT_DATA_TYPE_F32		0x0b	/**< 32-bit floating point */
#define POMP_PROT_DATA_TYPE_F64		0x0c	/**< 64-bit floating point */
#define POMP_PROT_DATA_TYPE_FD		0x0d	/**< File descriptor */

/** Size of protocol header */
#define POMP_PROT_HEADER_SIZE		12

/* Forward declaration */
struct pomp_prot;

/* Protocol functions */

struct pomp_prot *pomp_prot_new(void);

int pomp_prot_destroy(struct pomp_prot *prot);

ssize_t pomp_prot_decode_msg(struct pomp_prot *prot, const void *buf,
		size_t len, struct pomp_msg **msg);

int pomp_prot_release_msg(struct pomp_prot *prot, struct pomp_msg *msg);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_POMP_PROT_H_ */
