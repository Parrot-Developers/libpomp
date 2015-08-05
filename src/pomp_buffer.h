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
 *   * Neither the name of the <organization> nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
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
	size_t		size;		/**< Allocated size */
	size_t		len;		/**< Used length */
	uint32_t	fdcount;	/**< Number of fds put in buffer */

	/** Offsets in buffer where a file descriptor was put */
	size_t		fdoffs[POMP_BUFFER_MAX_FD_COUNT];
};

/**
 * Get the value of a file descriptor put in the buffer.
 * @param buf : buffer.
 * @param off : offset in buffer at which file descriptor is.
 * @return file descriptor or negative errno in case of error.
 */
static inline int pomp_buffer_get_fd(const struct pomp_buffer *buf, size_t off)
{
	int32_t v = 0;
	POMP_RETURN_ERR_IF_FAILED(buf != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(off + sizeof(v) <= buf->len, -EINVAL);

	/* Read value, don't care about byte order, it can only be used with
	 * local unix sockets */
	memcpy(&v, buf->data + off, sizeof(v));
	return v;
}

/**
 * Mark an offset in the buffer as holding a file descriptor.
 * @param buf : buffer.
 * @param off : offset in buffer at which file descriptor is.
 * @param fd : file descriptor value to set. It is not validated so invalid
 * values can be stored to record errors.
 * @return 0 in case of success, negative errno value in case of error.
 * -EPERM is returned if the buffer is shared (ref count is greater than 1).
 *
 * @remarks : in case of success, the buffer is owner of the file descriptor.
 */
static inline int pomp_buffer_register_fd(struct pomp_buffer *buf,
		size_t off, int fd)
{
	int32_t v = 0;
	POMP_RETURN_ERR_IF_FAILED(buf != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(buf->refcount <= 1, -EPERM);
	POMP_RETURN_ERR_IF_FAILED(off + sizeof(v) <= buf->len, -EINVAL);

	/* Make sure there isn't too many fds already put in buffer */
	if (buf->fdcount >= POMP_BUFFER_MAX_FD_COUNT) {
		POMP_LOGE("Too many file descriptors put in buffer");
		return -ENFILE;
	}

	/* Write value, don't care about byte order, it can only be used with
	 * local unix sockets */
	v = fd;
	memcpy(buf->data + off, &v, sizeof(v));

	/* Save offset */
	buf->fdoffs[buf->fdcount] = off;
	buf->fdcount++;
	return 0;
}

/**
 * Clear content of buffer.
 * @param buf : buffer.
 * @return 0 in case of success, negative errno value in case of error.
 * -EPERM is returned if the buffer is shared (ref count is greater than 1).
 */
static inline int pomp_buffer_clear(struct pomp_buffer *buf)
{
	uint32_t i = 0;
	int fd = 0;
	POMP_RETURN_ERR_IF_FAILED(buf != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(buf->refcount <= 1, -EPERM);

	/* Release file descriptors put in buffer */
	for (i = 0; i < buf->fdcount; i++) {
		if (buf->data == NULL) {
			POMP_LOGE("No internal data buffer");
		} else {
			fd = pomp_buffer_get_fd(buf, buf->fdoffs[i]);
			if (fd >= 0 && close(fd) < 0)
				POMP_LOG_FD_ERRNO("close", fd);
		}
	}
	buf->fdcount = 0;
	memset(buf->fdoffs, 0, sizeof(buf->fdoffs));

	/* Free internal data */
	free(buf->data);
	buf->data = NULL;
	buf->size = 0;
	buf->len = 0;
	return 0;
}

/**
 * Allocate a new buffer.
 * @return new buffer with initial ref count at 1 and no internal data or NULL
 * in case of error.
 */
static inline struct pomp_buffer *pomp_buffer_new(void)
{
	struct pomp_buffer *buf = NULL;

	/* Allocate buffer structure, set initial ref count to 1 */
	buf = calloc(1, sizeof(*buf));
	if (buf == NULL)
		return NULL;
	buf->refcount = 1;

	return buf;
}

/**
 * Create a new buffer with content copied from another buffer.
 * @param buf : buffer to copy.
 * @return new buffer with initial ref count at 1 and internal data copied from
 * given buffer or NULL in case of error.
 */
static inline struct pomp_buffer *pomp_buffer_new_copy(
		const struct pomp_buffer *buf)
{
	struct pomp_buffer *newbuf = NULL;
	uint32_t i = 0;
	size_t off = 0;
	int fd = 0, dupfd = 0;

	POMP_RETURN_VAL_IF_FAILED(buf != NULL, -EINVAL, NULL);

	/* Allocate buffer structure, set initial ref count to 1 */
	newbuf = calloc(1, sizeof(*newbuf));
	if (newbuf == NULL)
		goto error;
	newbuf->refcount = 1;

	if (buf->len != 0) {
		/* Allocate internal data */
		newbuf->data = malloc(buf->len);
		if (newbuf->data == NULL)
			goto error;

		/* Copy data */
		memcpy(newbuf->data, buf->data, buf->len);
		newbuf->size = buf->len;
		newbuf->len = buf->len;
	}

	/* Duplicate file descriptors */
	for (i = 0; i < buf->fdcount; i++) {
		off = buf->fdoffs[i];

		/* Get fd of input buffer */
		fd = pomp_buffer_get_fd(buf, off);
		if (fd < 0)
			goto error;

		/* Duplicate it */
		dupfd = dup(fd);
		if (dupfd < 0) {
			POMP_LOG_FD_ERRNO("dup", fd);
			goto error;
		}

		/* Set it in new buffer */
		if (pomp_buffer_register_fd(newbuf, off, dupfd) < 0) {
			close(dupfd);
			goto error;
		}
	}

	return newbuf;

	/* Cleanup in case of error */
error:
	if (newbuf != NULL) {
		(void)pomp_buffer_clear(newbuf);
		free(newbuf);
	}
	return NULL;
}

/**
 * Increase ref count of buffer.
 * @param buf : buffer.
 */
static inline void pomp_buffer_ref(struct pomp_buffer *buf)
{
	buf->refcount++;
}

/**
 * Decrease ref count of buffer. When it reaches 0, internal data as well as
 * buffer structure itself is freed.
 * @param buf : buffer.
 */
static inline void pomp_buffer_unref(struct pomp_buffer *buf)
{
	/* Free resource when ref count reaches 0 */
	if (--buf->refcount == 0) {
		(void)pomp_buffer_clear(buf);
		free(buf);
	}
}

/**
 * Resize internal data up to given size.
 * @param buf : buffer.
 * @param size : new size of buffer.
 * @return 0 in case of success, negative errno value in case of error.
 * -EPERM is returned if the buffer is shared (ref count is greater than 1).
 *
 * @remarks : internally the size will be aligned to POMP_BUFFER_ALLOC_STEP.
 */
static inline int pomp_buffer_resize(struct pomp_buffer *buf, size_t size)
{
	uint8_t *data = NULL;

	POMP_RETURN_ERR_IF_FAILED(buf != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(buf->refcount <= 1, -EPERM);

	/* Resize internal data if needed */
	if (size > buf->size) {
		size = POMP_BUFFER_ALIGN_ALLOC_SIZE(size);
		data = realloc(buf->data, size);
		if (data == NULL)
			return -ENOMEM;
		buf->data = data;
		buf->size = size;
	}
	return 0;
}

/**
 * Write data in buffer.
 * @param buf : buffer.
 * @param pos : write position. It will be updated in case of success.
 * @param p : pointer to data to write.
 * @param n : number of bytes to write.
 * @return 0 in case of success, negative errno value in case of error.
 * -EPERM is returned if the buffer is shared (ref count is greater than 1).
 */
static inline int pomp_buffer_write(struct pomp_buffer *buf, size_t *pos,
		const void *p, size_t n)
{
	int res = 0;

	POMP_RETURN_ERR_IF_FAILED(buf != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(pos != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(p != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(buf->refcount <= 1, -EPERM);

	/* Make sure there is enough room in data buffer */
	res = pomp_buffer_resize(buf, *pos + n);
	if (res < 0)
		return res;

	/* Copy data, adjust used length */
	memcpy(buf->data + *pos, p, n);
	*pos += n;
	if (*pos > buf->len)
		buf->len = *pos;
	return 0;
}

/**
 * Helper to write a single byte in buffer.
 * @param buf : buffer.
 * @param pos : write position. It will be updated in case of success.
 * @param b : byte to write.
 * @return 0 in case of success, negative errno value in case of error.
 * -EPERM is returned if the buffer is shared (ref count is greater than 1).
 */
static inline int pomp_buffer_writeb(struct pomp_buffer *buf, size_t *pos,
		uint8_t b)
{
	return pomp_buffer_write(buf, pos, &b, sizeof(b));
}

/**
 * Write a file descriptor in the buffer. It will be duplicated first.
 * @param buf : buffer.
 * @param pos : write position. It will be updated in case of success.
 * @return 0 in case of success, negative errno value in case of error.
 * -EPERM is returned if the buffer is shared (ref count is greater than 1).
 */
static inline int pomp_buffer_write_fd(struct pomp_buffer *buf, size_t *pos,
		int fd)
{
	int res = 0;
	int dupfd = -1;
	int32_t dummy = 0;
	size_t off = 0;
	POMP_RETURN_ERR_IF_FAILED(buf != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(pos != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(buf->refcount <= 1, -EPERM);
	POMP_RETURN_ERR_IF_FAILED(fd >= 0, -EINVAL);

	/* Remember position at which fd will be written, write a dummy value */
	off = *pos;
	res = pomp_buffer_write(buf, pos, &dummy, sizeof(dummy));
	if (res < 0)
		goto error;

	/* Duplicate file descriptor */
	dupfd = dup(fd);
	if (dupfd < 0) {
		res = -errno;
		POMP_LOG_FD_ERRNO("dup", fd);
		goto error;
	}

	/* Register fd in buffer */
	res = pomp_buffer_register_fd(buf, off, dupfd);
	if (res < 0)
		goto error;

	return 0;

	/* Cleanup in case of error */
error:
	if (dupfd >= 0)
		close(dupfd);
	return res;
}

/**
 * Read data from buffer.
 * @param buf : buffer.
 * @param pos : read position. It will be updated in case of success.
 * @param p : pointer to data to read.
 * @param n : number of bytes to read.
 * @return 0 in case of success, negative errno value in case of error.
 */
static inline int pomp_buffer_read(const struct pomp_buffer *buf, size_t *pos,
		void *p, size_t n)
{
	POMP_RETURN_ERR_IF_FAILED(buf != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(pos != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(p != NULL, -EINVAL);

	/* Make sure there is enough room in data buffer */
	POMP_RETURN_ERR_IF_FAILED(*pos + n <= buf->len, -EINVAL);

	/* Copy data */
	memcpy(p, buf->data + *pos, n);
	*pos += n;
	return 0;
}

/**
 * Read data from buffer without copy.
 * @param buf : buffer.
 * @param pos : read position. It will be updated in case of success.
 * @param p : will receive pointer to data inside buffer. It is valid as long
 * as the buffer is valid and no write or resize operation is performed.
 * @param n : number of bytes to read.
 * @return 0 in case of success, negative errno value in case of error.
 */
static inline int pomp_buffer_cread(const struct pomp_buffer *buf, size_t *pos,
		const void **p, size_t n)
{
	POMP_RETURN_ERR_IF_FAILED(buf != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(pos != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(p != NULL, -EINVAL);

	/* Make sure there is enough room in data buffer */
	POMP_RETURN_ERR_IF_FAILED(*pos + n <= buf->len, -EINVAL);

	/* Simply set start of data */
	*p = buf->data + *pos;
	*pos += n;
	return 0;
}

/**
 * Helper to read a single byte from buffer.
 * @param buf : buffer.
 * @param pos : read position. It will be updated in case of success.
 * @param b : byte to read.
 * @return 0 in case of success, negative errno value in case of error.
 */
static inline int pomp_buffer_readb(const struct pomp_buffer *buf, size_t *pos,
		uint8_t *b)
{
	return pomp_buffer_read(buf, pos, b, sizeof(*b));
}

/**
 * Read a file descriptor from the buffer.
 * @param buf : buffer.
 * @param pos : read position. It will be updated in case of success.
 * @param fd : file descriptor to read.
 * @return 0 in case of success, negative errno value in case of error.
 */
static inline int pomp_buffer_read_fd(struct pomp_buffer *buf, size_t *pos,
		int *fd)
{
	int res = 0;
	uint32_t i = 0;
	POMP_RETURN_ERR_IF_FAILED(buf != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(pos != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(fd != NULL, -EINVAL);

	/* First, make sure that the position really holds a file descriptor */
	for (i = 0; i < buf->fdcount; i++) {
		if (buf->fdoffs[i] == *pos) {
			res = pomp_buffer_get_fd(buf, *pos);
			if (res < 0)
				return res;
			*fd = res;
			*pos += sizeof(int32_t);
			return 0;
		}
	}

	POMP_LOGE("No file descriptor at given position");
	return -EINVAL;
}

#endif /* !_POMP_BUFFER_H_ */
