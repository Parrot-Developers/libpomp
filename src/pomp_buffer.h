/**
 * @file pomp_buffer.h
 *
 * @brief Manage a reference counted buffer with automatic resizing.
 *
 * When the buffer is shared (reference count is greater than 1), it becomes
 * read-only, all write and resize operations will fail with -EPERM.
 *
 * All read and write operations take a position parameter that is updated
 * during the call. The buffer structure does not maintain any position
 * internally so all operations can be mixed without problems.
 * The buffer structure only stores the data, and size used.
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

#ifndef _POMP_BUFFER_H_
#define _POMP_BUFFER_H_

/** Allocation step in buffer (shall be a power of 2) */
#define POMP_BUFFER_ALLOC_STEP	(256u)

/** Align size up to next allocation step */
#define POMP_BUFFER_ALIGN_ALLOC_SIZE(_x) \
	(((_x) + POMP_BUFFER_ALLOC_STEP - 1) & (~(POMP_BUFFER_ALLOC_STEP - 1)))

/** Maximum number of file descriptor that can be put in a buffer */
#define POMP_BUFFER_MAX_FD_COUNT	4

/** Reference counted buffer */
struct pomp_buffer {
	uint32_t	refcount;	/**< Reference count */
	uint8_t		*data;		/**< Allocated data */
	size_t		capacity;	/**< Allocated size */
	size_t		len;		/**< Used length */
	uint32_t	fdcount;	/**< Number of fds put in buffer */

	/** Offsets in buffer where a file descriptor was put */
	size_t		fdoffs[POMP_BUFFER_MAX_FD_COUNT];
};

int pomp_buffer_get_fd(const struct pomp_buffer *buf, size_t off);

int pomp_buffer_register_fd(struct pomp_buffer *buf, size_t off, int fd);

int pomp_buffer_clear(struct pomp_buffer *buf);

int pomp_buffer_writeb(struct pomp_buffer *buf, size_t *pos, uint8_t b);

int pomp_buffer_write_fd(struct pomp_buffer *buf, size_t *pos, int fd);

int pomp_buffer_readb(const struct pomp_buffer *buf, size_t *pos, uint8_t *b);

int pomp_buffer_read_fd(struct pomp_buffer *buf, size_t *pos, int *fd);

#endif /* !_POMP_BUFFER_H_ */
