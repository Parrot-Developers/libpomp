/**
 * @file pomp_buffer.c
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

#include "pomp_priv.h"

/**
 * Get the value of a file descriptor put in the buffer.
 * @param buf : buffer.
 * @param off : offset in buffer at which file descriptor is.
 * @return file descriptor or negative errno in case of error.
 */
int pomp_buffer_get_fd(const struct pomp_buffer *buf, size_t off)
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
int pomp_buffer_register_fd(struct pomp_buffer *buf, size_t off, int fd)
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
int pomp_buffer_clear(struct pomp_buffer *buf)
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
	buf->capacity = 0;
	buf->len = 0;
	return 0;
}

/*
 * See documentation in public header.
 */
struct pomp_buffer *pomp_buffer_new(size_t capacity)
{
	struct pomp_buffer *buf = NULL;

	/* Allocate buffer structure, set initial ref count to 1 */
	buf = calloc(1, sizeof(*buf));
	if (buf == NULL)
		return NULL;
	buf->refcount = 1;

	/* Set initial capacity */
	if (capacity != 0 && pomp_buffer_set_capacity(buf, capacity) < 0) {
		free(buf);
		return NULL;
	}

	return buf;
}

/*
 * See documentation in public header.
 */
struct pomp_buffer *pomp_buffer_new_copy(const struct pomp_buffer *buf)
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
		newbuf->capacity = buf->len;
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

/*
 * See documentation in public header.
 */
struct pomp_buffer *pomp_buffer_new_with_data(const void *data, size_t len)
{
	struct pomp_buffer *buf = pomp_buffer_new(len);
	if (buf != NULL) {
		memcpy(buf->data, data, len);
		buf->len = len;
	}
	return buf;
}

/*
 * See documentation in public header.
 */
struct pomp_buffer *pomp_buffer_new_get_data(size_t capacity, void **data)
{
	struct pomp_buffer *buf = pomp_buffer_new(capacity);
	if (buf != NULL && data != NULL)
		*data = buf->data;
	return buf;
}

/*
 * See documentation in public header.
 */
void pomp_buffer_ref(struct pomp_buffer *buf)
{
#if defined(__GNUC__)
	__sync_add_and_fetch(&buf->refcount, 1);
#elif defined(_WIN32)
	/* codecheck_ignore[SPACING,VOLATILE] */
	InterlockedIncrement((long volatile *)&buf->refcount);
#else
#error No atomic increment function found on this platform
#endif
}

/*
 * See documentation in public header.
 */
void pomp_buffer_unref(struct pomp_buffer *buf)
{
	uint32_t res = 0;
#if defined(__GNUC__)
	res = __sync_sub_and_fetch(&buf->refcount, 1);
#elif defined(_WIN32)
	/* codecheck_ignore[SPACING,VOLATILE] */
	res = (uint32_t)InterlockedDecrement((long volatile *)&buf->refcount);
#else
#error No atomic decrement function found on this platform
#endif

	/* Free resource when ref count reaches 0 */
	if (res == 0) {
		(void)pomp_buffer_clear(buf);
		free(buf);
	}
}

/*
 * See documentation in public header.
 */
int pomp_buffer_is_shared(struct pomp_buffer *buf)
{
	return buf != NULL && buf->refcount > 1;
}

/*
 * See documentation in public header.
 */
int pomp_buffer_set_capacity(struct pomp_buffer *buf, size_t capacity)
{
	uint8_t *data = NULL;
	POMP_RETURN_ERR_IF_FAILED(buf != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(capacity >= buf->len, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(buf->refcount <= 1, -EPERM);

	/* Resize internal data */
	data = realloc(buf->data, capacity);
	if (data == NULL)
		return -ENOMEM;
	buf->data = data;
	buf->capacity = capacity;
	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_buffer_set_len(struct pomp_buffer *buf, size_t len)
{
	POMP_RETURN_ERR_IF_FAILED(buf != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(len <= buf->capacity, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(buf->refcount <= 1, -EPERM);
	buf->len = len;
	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_buffer_get_data(struct pomp_buffer *buf,
		void **data, size_t *len, size_t *capacity)
{
	POMP_RETURN_ERR_IF_FAILED(buf != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(buf->refcount <= 1, -EPERM);
	if (data != NULL)
		*data = buf->data;
	if (len != NULL)
		*len = buf->len;
	if (capacity != NULL)
		*capacity = buf->capacity;
	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_buffer_get_cdata(struct pomp_buffer *buf,
		const void **cdata, size_t *len, size_t *capacity)
{
	POMP_RETURN_ERR_IF_FAILED(buf != NULL, -EINVAL);
	if (cdata != NULL)
		*cdata = buf->data;
	if (len != NULL)
		*len = buf->len;
	if (capacity != NULL)
		*capacity = buf->capacity;
	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_buffer_append_data(struct pomp_buffer *buf,
		const void *data, size_t len)
{
	size_t pos;

	POMP_RETURN_ERR_IF_FAILED(buf != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(data != NULL, -EINVAL);

	pos = buf->len;
	return pomp_buffer_write(buf, &pos, data, len);
}

/*
 * See documentation in public header.
 */
int pomp_buffer_ensure_capacity(struct pomp_buffer *buf, size_t capacity)
{
	POMP_RETURN_ERR_IF_FAILED(buf != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(buf->refcount <= 1, -EPERM);

	/* Resize internal data if needed */
	if (capacity > buf->capacity) {
		capacity = POMP_BUFFER_ALIGN_ALLOC_SIZE(capacity);
		return pomp_buffer_set_capacity(buf, capacity);
	}
	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_buffer_write(struct pomp_buffer *buf, size_t *pos,
		const void *p, size_t n)
{
	int res = 0;

	POMP_RETURN_ERR_IF_FAILED(buf != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(pos != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(p != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(buf->refcount <= 1, -EPERM);

	/* Make sure there is enough room in data buffer */
	res = pomp_buffer_ensure_capacity(buf, *pos + n);
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
int pomp_buffer_writeb(struct pomp_buffer *buf, size_t *pos, uint8_t b)
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
int pomp_buffer_write_fd(struct pomp_buffer *buf, size_t *pos, int fd)
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

/*
 * See documentation in public header.
 */
int pomp_buffer_read(const struct pomp_buffer *buf, size_t *pos,
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

/*
 * See documentation in public header.
 */
int pomp_buffer_cread(const struct pomp_buffer *buf, size_t *pos,
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
int pomp_buffer_readb(const struct pomp_buffer *buf, size_t *pos, uint8_t *b)
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
int pomp_buffer_read_fd(struct pomp_buffer *buf, size_t *pos, int *fd)
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
