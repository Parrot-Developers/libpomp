/**
 * @file pomp_test_basic.c
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

#include "pomp_test.h"

#define TEST_MSGID	(42)

#define TEST_VAL_I8	((int8_t)(-32))
#define TEST_VAL_U8	((uint8_t)(212u))
#define TEST_VAL_I16	((int16_t)(-1000))
#define TEST_VAL_U16	((uint16_t)(23000u))
#define TEST_VAL_I32	((int32_t)(-71000))
#define TEST_VAL_U32	((uint32_t)(3000000000u))
#define TEST_VAL_I64	((int64_t)(-4000000000ll))
#define TEST_VAL_U64	((uint64_t)(10000000000000000000ull))
#define TEST_VAL_STR	"Hello World !!!"
#define TEST_VAL_BUF	"hELLO wORLD ???"
#define TEST_VAL_BUFLEN	15
#define TEST_VAL_BUFCMD "0123456789ABCDEFFEDCBA9876543210"
#define TEST_VAL_BUFCMDLEN 16
#define TEST_VAL_BUF_MEM (uint8_t []){0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}
#define TEST_VAL_F32	(3.1415927410125732421875)
#define TEST_VAL_F64	(3.141592653589793115997963468544185161590576171875)

#define TEST_VAL_I8_ENCODED	0xe0
#define TEST_VAL_U8_ENCODED	0xd4
#define TEST_VAL_I16_ENCODED	0x18, 0xfc
#define TEST_VAL_U16_ENCODED	0xd8, 0x59
#define TEST_VAL_I32_ENCODED	0xaf, 0xd5, 0x08
#define TEST_VAL_U32_ENCODED	0x80, 0xbc, 0xc1, 0x96, 0x0b
#define TEST_VAL_I64_ENCODED	0xff, 0x9f, 0xd9, 0xe6, 0x1d
#define TEST_VAL_U64_ENCODED	0x80, 0x80, 0xa0, 0xcf, 0xc8, 0xe0, 0xc8, 0xe3, 0x8a, 0x01
#define TEST_VAL_STR_ENCODED	0x10, 'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', ' ', '!', '!', '!', 0x00
#define TEST_VAL_BUF_ENCODED	0x0f, 'h', 'E', 'L', 'L', 'O', ' ', 'w', 'O', 'R', 'L', 'D', ' ', '?', '?', '?'
#define TEST_VAL_BUFCMD_ENCODED 0x10, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10
#define TEST_VAL_F32_ENCODED	0xdb, 0x0f, 0x49, 0x40
#define TEST_VAL_F64_ENCODED	0x18, 0x2d, 0x44, 0x54, 0xfb, 0x21, 0x09, 0x40

#define GET_BYTE(_val, _i) \
	(((uint64_t)(_val) >> (8 * (_i))) & 0xff)

/** */
struct test_data {
	int8_t		i8;
	uint8_t		u8;
	int16_t		i16;
	uint16_t	u16;
	int32_t		i32;
	uint32_t	u32;
	int64_t		i64;
	uint64_t	u64;
	const char	*cstr;
	char		*str;
	void		*buf;
	const void	*cbuf;
	uint32_t	buflen;
	float		f32;
	double		f64;
};

/** */
static const struct test_data s_refdata = {
	.i8 = TEST_VAL_I8,
	.u8 = TEST_VAL_U8,
	.i16 = TEST_VAL_I16,
	.u16 = TEST_VAL_U16,
	.i32 = TEST_VAL_I32,
	.u32 = TEST_VAL_U32,
	.i64 = TEST_VAL_I64,
	.u64 = TEST_VAL_U64,
	.cstr = TEST_VAL_STR,
	.str = NULL,
	.cbuf = TEST_VAL_BUF,
	.buflen = TEST_VAL_BUFLEN,
	.f32 = TEST_VAL_F32,
	.f64 = TEST_VAL_F64,
};

/** */
static const uint8_t s_refdata_enc[] = {
	0x01, TEST_VAL_I8_ENCODED,
	0x02, TEST_VAL_U8_ENCODED,
	0x03, TEST_VAL_I16_ENCODED,
	0x04, TEST_VAL_U16_ENCODED,
	0x05, TEST_VAL_I32_ENCODED,
	0x06, TEST_VAL_U32_ENCODED,
	0x07, TEST_VAL_I64_ENCODED,
	0x08, TEST_VAL_U64_ENCODED,
	0x09, TEST_VAL_STR_ENCODED,
	0x0a, TEST_VAL_BUF_ENCODED,
	0x0b, TEST_VAL_F32_ENCODED,
	0x0c, TEST_VAL_F64_ENCODED,
};
#define REFDATA_ENC_SIZE sizeof(s_refdata_enc)

/** */
static const uint8_t s_refdata_enc_argv[] = {
	0x01, TEST_VAL_I8_ENCODED,
	0x02, TEST_VAL_U8_ENCODED,
	0x03, TEST_VAL_I16_ENCODED,
	0x04, TEST_VAL_U16_ENCODED,
	0x05, TEST_VAL_I32_ENCODED,
	0x06, TEST_VAL_U32_ENCODED,
	0x07, TEST_VAL_I64_ENCODED,
	0x08, TEST_VAL_U64_ENCODED,
	0x09, TEST_VAL_STR_ENCODED,
	0x0a, TEST_VAL_BUFCMD_ENCODED,
	0x0b, TEST_VAL_F32_ENCODED,
	0x0c, TEST_VAL_F64_ENCODED,
};
#define REFDATA_ENC_ARGV_SIZE sizeof(s_refdata_enc_argv)

/** */
static const uint8_t s_refdata_enc_header[] = {
	'P', 'O', 'M', 'P',
	GET_BYTE(TEST_MSGID, 0), GET_BYTE(TEST_MSGID, 1),
	GET_BYTE(TEST_MSGID, 2), GET_BYTE(TEST_MSGID, 3),
	GET_BYTE(REFDATA_ENC_SIZE + 12, 0), GET_BYTE(REFDATA_ENC_SIZE + 12, 1),
	GET_BYTE(REFDATA_ENC_SIZE + 12, 2), GET_BYTE(REFDATA_ENC_SIZE + 12, 3)
};

/** */
static const uint8_t s_refdata_enc_header_no_payload[] = {
	'P', 'O', 'M', 'P',
	GET_BYTE(TEST_MSGID, 0), GET_BYTE(TEST_MSGID, 1),
	GET_BYTE(TEST_MSGID, 2), GET_BYTE(TEST_MSGID, 3),
	GET_BYTE(12, 0), GET_BYTE(12, 1),
	GET_BYTE(12, 2), GET_BYTE(12, 3)
};

/** */
static const char *s_msg_dump =
	"{"
	"ID:42"
	", I8:-32"
	", U8:212"
	", I16:-1000"
	", U16:23000"
	", I32:-71000"
	", U32:3000000000"
	", I64:-4000000000"
	", U64:10000000000000000000"
	", STR:'Hello World !!!'"
	", BUF:"
	", F32:3.141593"
	", F64:3.141593"
	"}";

#define REFDATA_ARGV_ADD(_valfmt, _val) \
	snprintf(bufs[i], sizeof(bufs[i]), _valfmt, _val); \
	argv[i] = bufs[i]; \
	i++;

/** */
static const char **get_refdata_argv(int *argc)
{
	int i = 0;
	static char bufs[18][256];
	static const char *argv[18];

	REFDATA_ARGV_ADD("%hhi", TEST_VAL_I8);
	REFDATA_ARGV_ADD("%hhu", TEST_VAL_U8);
	REFDATA_ARGV_ADD("%hi", TEST_VAL_I16);
	REFDATA_ARGV_ADD("%hu", TEST_VAL_U16);
	REFDATA_ARGV_ADD("%i", TEST_VAL_I32);
	REFDATA_ARGV_ADD("%u", TEST_VAL_U32);
	REFDATA_ARGV_ADD("%" PRIi64, (int64_t)TEST_VAL_I64);
	REFDATA_ARGV_ADD("%" PRIu64, (uint64_t)TEST_VAL_U64);
	REFDATA_ARGV_ADD("%s", TEST_VAL_STR);
	REFDATA_ARGV_ADD("%s", TEST_VAL_BUFCMD);
	REFDATA_ARGV_ADD("%u", TEST_VAL_BUFCMDLEN);
	REFDATA_ARGV_ADD("%.32f", TEST_VAL_F32);
	REFDATA_ARGV_ADD("%.64f", TEST_VAL_F64);

	argv[i] = NULL;
	*argc = i;
	return argv;
}

/** */
static void test_buffer_base(void)
{
	int res = 0;
	struct pomp_buffer *buf = NULL;
	struct pomp_buffer *buf2 = NULL;
	struct pomp_buffer *buf3 = NULL;
	struct pomp_buffer *buf4 = NULL;
	struct pomp_buffer *buf5 = NULL;
	void *data = NULL;
	const void *cdata = NULL;
	size_t len = 0, capacity = 0;

	/* Allocation, ref, unref */
	buf = pomp_buffer_new(0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
	CU_ASSERT_EQUAL(buf->capacity, 0);
	CU_ASSERT_EQUAL(buf->len, 0);
	CU_ASSERT_EQUAL(buf->refcount, 1);
	CU_ASSERT_EQUAL(pomp_buffer_is_shared(buf), 0);
	pomp_buffer_ref(buf);
	CU_ASSERT_EQUAL(buf->refcount, 2);
	CU_ASSERT_EQUAL(pomp_buffer_is_shared(buf), 1);
	pomp_buffer_unref(buf);
	CU_ASSERT_EQUAL(buf->refcount, 1);
	CU_ASSERT_EQUAL(pomp_buffer_is_shared(buf), 0);

	/* Copy (without data) */
	buf2 = pomp_buffer_new_copy(buf);
	CU_ASSERT_PTR_NOT_NULL_FATAL(buf2);
	CU_ASSERT_EQUAL(buf2->refcount, 1);

	/* Invalid copy (NULL param) */
	buf3 = pomp_buffer_new_copy(NULL);
	CU_ASSERT_PTR_NULL(buf3);

	/* Resize */
	res = pomp_buffer_ensure_capacity(buf, 1000);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_PTR_NOT_NULL(buf->data);
	CU_ASSERT_TRUE(buf->capacity >= 1000);

	/* Invalid resize (NULL param) */
	res = pomp_buffer_ensure_capacity(NULL, 1000);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid clear (NULL param) */
	res = pomp_buffer_clear(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Clear */
	res = pomp_buffer_clear(buf);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(buf->capacity, 0);
	CU_ASSERT_EQUAL(buf->len, 0);

	/* Allocation with initial capacity */
	buf3 = pomp_buffer_new(100);
	CU_ASSERT_PTR_NOT_NULL_FATAL(buf3);
	CU_ASSERT_EQUAL(buf3->capacity, 100);
	CU_ASSERT_EQUAL(buf3->len, 0);

	/* Allocation with initial capacity, and data access */
	buf4 = pomp_buffer_new_get_data(100, &data);
	CU_ASSERT_PTR_NOT_NULL_FATAL(buf4);
	CU_ASSERT_EQUAL(buf4->capacity, 100);
	CU_ASSERT_EQUAL(buf4->len, 0);
	CU_ASSERT_EQUAL(buf4->data, data);

	/* Allocation with initial data */
	buf5 = pomp_buffer_new_with_data("Hello", 5);
	CU_ASSERT_PTR_NOT_NULL_FATAL(buf5);
	CU_ASSERT_EQUAL(buf5->len, 5);
	CU_ASSERT_EQUAL(memcmp(buf5->data, "Hello", 5), 0);

	/* Length tests */
	res = pomp_buffer_set_len(buf3, 10);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(buf3->len, 10);
	res = pomp_buffer_set_len(NULL, 200);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_buffer_set_len(buf3, 200);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Capacity tests */
	res = pomp_buffer_set_capacity(buf3, 200);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(buf3->capacity, 200);
	res = pomp_buffer_set_capacity(buf3, 20);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(buf3->capacity, 20);
	res = pomp_buffer_set_capacity(NULL, 5);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_buffer_set_capacity(buf3, 5);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Data retrieval */
	res = pomp_buffer_get_data(buf3, &data, &len, &capacity);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(data, buf3->data);
	CU_ASSERT_EQUAL(len, buf3->len);
	CU_ASSERT_EQUAL(capacity, buf3->capacity);
	res = pomp_buffer_get_cdata(buf3, &cdata, &len, &capacity);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(cdata, buf3->data);
	CU_ASSERT_EQUAL(len, buf3->len);
	CU_ASSERT_EQUAL(capacity, buf3->capacity);
	res = pomp_buffer_get_data(buf3, NULL, &len, &capacity);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_buffer_get_data(buf3, &data, NULL, &capacity);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_buffer_get_data(buf3, &data, &len, NULL);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_buffer_get_cdata(buf3, NULL, &len, &capacity);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_buffer_get_cdata(buf3, &cdata, NULL, &capacity);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_buffer_get_cdata(buf3, &cdata, &len, NULL);
	CU_ASSERT_EQUAL(res, 0);

	/* Data retrieval invalid params */
	res = pomp_buffer_get_data(NULL, &data, &len, &capacity);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_buffer_get_cdata(NULL, &cdata, &len, &capacity);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Release */
	pomp_buffer_unref(buf);
	pomp_buffer_unref(buf2);
	pomp_buffer_unref(buf3);
	pomp_buffer_unref(buf4);
	pomp_buffer_unref(buf5);
}

/** */
static void test_buffer_read_write(void)
{
	static const uint8_t refdata[4] = {0x11, 0x22, 0x33, 0x44};

	int res = 0;
	size_t pos = 0;
	uint8_t data[4];
	const uint8_t *cdata = NULL;
	struct pomp_buffer *buf = NULL, *buf2 = NULL;

	buf = pomp_buffer_new(0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(buf);

	/* Write at position 0 */
	pos = 0;
	res = pomp_buffer_write(buf, &pos, refdata, 4);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(pos, 4);
	CU_ASSERT_EQUAL(buf->len, 4);
	CU_ASSERT_TRUE(buf->capacity >= 4);
	CU_ASSERT_PTR_NOT_NULL_FATAL(buf->data);
	CU_ASSERT_EQUAL(buf->data[0], refdata[0]);
	CU_ASSERT_EQUAL(buf->data[1], refdata[1]);
	CU_ASSERT_EQUAL(buf->data[2], refdata[2]);
	CU_ASSERT_EQUAL(buf->data[3], refdata[3]);

	/* Write at position 1000 */
	pos = 1000;
	res = pomp_buffer_write(buf, &pos, refdata, 4);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(pos, 1004);
	CU_ASSERT_EQUAL(buf->len, 1004);
	CU_ASSERT_TRUE(buf->capacity >= 1004);
	CU_ASSERT_PTR_NOT_NULL_FATAL(buf->data);
	CU_ASSERT_EQUAL(buf->data[1000], refdata[0]);
	CU_ASSERT_EQUAL(buf->data[1001], refdata[1]);
	CU_ASSERT_EQUAL(buf->data[1002], refdata[2]);
	CU_ASSERT_EQUAL(buf->data[1003], refdata[3]);

	/* Read at position 0 */
	memset(data, 0, sizeof(data));
	pos = 0;
	res = pomp_buffer_read(buf, &pos, data, 4);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(pos, 4);
	CU_ASSERT_EQUAL(data[0], refdata[0]);
	CU_ASSERT_EQUAL(data[1], refdata[1]);
	CU_ASSERT_EQUAL(data[2], refdata[2]);
	CU_ASSERT_EQUAL(data[3], refdata[3]);

	/* Read at position 0, no copy */
	cdata = NULL;
	pos = 0;
	res = pomp_buffer_cread(buf, &pos, (const void **)&cdata, 4);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_NOT_EQUAL_FATAL(cdata, NULL);
	CU_ASSERT_EQUAL(pos, 4);
	CU_ASSERT_EQUAL(cdata[0], refdata[0]);
	CU_ASSERT_EQUAL(cdata[1], refdata[1]);
	CU_ASSERT_EQUAL(cdata[2], refdata[2]);
	CU_ASSERT_EQUAL(cdata[3], refdata[3]);

	/* Read at position 1000 */
	memset(data, 0, sizeof(data));
	pos = 1000;
	res = pomp_buffer_read(buf, &pos, data, 4);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(pos, 1004);
	CU_ASSERT_EQUAL(data[0], refdata[0]);
	CU_ASSERT_EQUAL(data[1], refdata[1]);
	CU_ASSERT_EQUAL(data[2], refdata[2]);
	CU_ASSERT_EQUAL(data[3], refdata[3]);

	/* Read at position 1000, no copy */
	cdata = NULL;
	pos = 1000;
	res = pomp_buffer_cread(buf, &pos, (const void **)&cdata, 4);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_NOT_EQUAL_FATAL(cdata, NULL);
	CU_ASSERT_EQUAL(pos, 1004);
	CU_ASSERT_EQUAL(cdata[0], refdata[0]);
	CU_ASSERT_EQUAL(cdata[1], refdata[1]);
	CU_ASSERT_EQUAL(cdata[2], refdata[2]);
	CU_ASSERT_EQUAL(cdata[3], refdata[3]);

	/* Copy (with data) */
	buf2 = pomp_buffer_new_copy(buf);
	CU_ASSERT_PTR_NOT_NULL_FATAL(buf2);
	CU_ASSERT_EQUAL(buf2->refcount, 1);

	/* Invalid write (NULL param) */
	pos = 0;
	res = pomp_buffer_write(NULL, &pos, refdata, 4);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_buffer_write(buf, NULL, refdata, 4);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_buffer_write(buf, &pos, NULL, 4);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid read (NULL param) */
	pos = 0;
	res = pomp_buffer_read(NULL, &pos, data, 4);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_buffer_read(buf, NULL, data, 4);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_buffer_read(buf, &pos, NULL, 4);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid read, no copy (NULL param) */
	pos = 0;
	res = pomp_buffer_cread(NULL, &pos, (const void **)&cdata, 4);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_buffer_cread(buf, NULL, (const void **)&cdata, 4);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_buffer_cread(buf, &pos, NULL, 4);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid read (wrong pos) */
	pos = 2000;
	res = pomp_buffer_read(buf, &pos, data, 4);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid read, no copy (wrong pos) */
	pos = 2000;
	res = pomp_buffer_cread(buf, &pos, (const void **)&cdata, 4);
	CU_ASSERT_EQUAL(res, -EINVAL);

	pomp_buffer_unref(buf);
	pomp_buffer_unref(buf2);
}

/** */
static void test_buffer_perm(void)
{
	static const uint8_t cdata[4] = {0x11, 0x22, 0x33, 0x44};
	void *data = NULL;
	size_t len = 0, capacity = 0;

	int res = 0;
	size_t pos = 0;
	struct pomp_buffer *buf = NULL;

	/* Create buffer with 2 ref */
	buf = pomp_buffer_new(20);
	CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
	pomp_buffer_ref(buf);

	/* Not permitted clear */
	res = pomp_buffer_clear(buf);
	CU_ASSERT_EQUAL(res, -EPERM);

	/* Not permitted resize */
	res = pomp_buffer_ensure_capacity(buf, 100);
	CU_ASSERT_EQUAL(res, -EPERM);
	res = pomp_buffer_set_capacity(buf, 100);
	CU_ASSERT_EQUAL(res, -EPERM);

	/* Not permitted set length */
	res = pomp_buffer_set_len(buf, 20);
	CU_ASSERT_EQUAL(res, -EPERM);

	/* Not permitted write */
	pos = 0;
	res = pomp_buffer_write(buf, &pos, cdata, 4);
	CU_ASSERT_EQUAL(res, -EPERM);

	/* Not permitted data retrieval (not constant) */
	res = pomp_buffer_get_data(buf, &data, &len, &capacity);
	CU_ASSERT_EQUAL(res, -EPERM);

	pomp_buffer_unref(buf);
	pomp_buffer_unref(buf);
}

/** */
static void test_buffer_fd(void)
{
#ifndef _WIN32
	int res = 0, fd = -1, fd2 = -1;
	uint32_t i = 0;
	struct pomp_buffer *buf = NULL;
	struct pomp_buffer *buf2 = NULL;
	int fds[5][2];
	size_t pos = 0;
	struct stat st1, st2, st3;

	/* Create file descriptors */
	for (i = 0; i < 5; i++) {
		res = pipe(fds[i]);
		CU_ASSERT_EQUAL(res, 0);
		CU_ASSERT_TRUE(fds[i][0] >= 0);
		CU_ASSERT_TRUE(fds[i][1] >= 0);
	}

	/* Create buffer */
	buf = pomp_buffer_new(0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
	res = pomp_buffer_ensure_capacity(buf, 100);
	CU_ASSERT_EQUAL(res, 0);

	/* Write 4 file descriptors */
	for (i = 0; i < 4; i++) {
		pos = 10 * (i + 1);
		res = pomp_buffer_write_fd(buf, &pos, fds[i][0]);
		CU_ASSERT_EQUAL(res, 0);
		CU_ASSERT_EQUAL(buf->fdcount, i + 1);
	}

	/* 5th write should get an error */
	pos = 50;
	res = pomp_buffer_write_fd(buf, &pos, fds[4][0]);
	CU_ASSERT_EQUAL(res, -ENFILE);

	/* Read file descriptors */
	for (i = 0; i < 4; i++) {
		pos = 10 * (i + 1);
		res = pomp_buffer_read_fd(buf, &pos, &fd);
		CU_ASSERT_EQUAL(res, 0);
		CU_ASSERT_TRUE(fd >= 0);

		res = fstat(fds[i][0], &st1);
		CU_ASSERT_EQUAL(res, 0);
		res = fstat(fd, &st2);
		CU_ASSERT_EQUAL(res, 0);

		/* file descriptor should have been duplicated so value is
		 * not the same but it it shall be the same inode */
		CU_ASSERT_NOT_EQUAL(fd, fds[i][0]);
		CU_ASSERT_EQUAL(st1.st_ino, st2.st_ino);
	}

	/* 5th read shall be invalid */
	pos = 50;
	res = pomp_buffer_read_fd(buf, &pos, &fd);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid write (NULL param) */
	pos = 50;
	res = pomp_buffer_write_fd(NULL, &pos, fds[4][0]);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_buffer_write_fd(buf, NULL, fds[4][0]);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_buffer_write_fd(buf, &pos, -1);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid read (NULL param) */
	pos = 50;
	res = pomp_buffer_read_fd(NULL, &pos, &fd);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_buffer_read_fd(buf, NULL, &fd);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_buffer_read_fd(buf, &pos, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid arguments */
	pos = 50;
	res = pomp_buffer_register_fd(NULL, pos, fds[4][0]);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_buffer_register_fd(buf, 98, fds[4][0]);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_buffer_get_fd(NULL, pos);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_buffer_get_fd(buf, 98);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Permission denied when shared */
	pomp_buffer_ref(buf);
	pos = 50;
	res = pomp_buffer_register_fd(buf, pos, fds[4][0]);
	CU_ASSERT_EQUAL(res, -EPERM);
	pomp_buffer_unref(buf);

	/* Copy */
	buf2 = pomp_buffer_new_copy(buf);
	CU_ASSERT_PTR_NOT_NULL_FATAL(buf);

	/* Read file descriptors */
	for (i = 0; i < 4; i++) {
		pos = 10 * (i + 1);
		res = pomp_buffer_read_fd(buf, &pos, &fd);
		CU_ASSERT_EQUAL(res, 0);
		CU_ASSERT_TRUE(fd >= 0);

		pos = 10 * (i + 1);
		res = pomp_buffer_read_fd(buf2, &pos, &fd2);
		CU_ASSERT_EQUAL(res, 0);
		CU_ASSERT_TRUE(fd2 >= 0);

		res = fstat(fds[i][0], &st1);
		CU_ASSERT_EQUAL(res, 0);
		res = fstat(fd, &st2);
		CU_ASSERT_EQUAL(res, 0);
		res = fstat(fd2, &st3);
		CU_ASSERT_EQUAL(res, 0);

		/* file descriptor should have been duplicated so value is
		 * not the same but it it shall be the same inode */
		CU_ASSERT_NOT_EQUAL(fd, fds[i][0]);
		CU_ASSERT_NOT_EQUAL(fd2, fds[i][0]);
		CU_ASSERT_NOT_EQUAL(fd, fd2);
		CU_ASSERT_EQUAL(st1.st_ino, st2.st_ino);
		CU_ASSERT_EQUAL(st1.st_ino, st3.st_ino);
	}

	/* Clear */
	res = pomp_buffer_clear(buf);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(buf->fdcount, 0);

	/* Try to write an already closed file descriptor */
	res = close(fds[4][0]); CU_ASSERT_EQUAL(res, 0);
	res = close(fds[4][1]); CU_ASSERT_EQUAL(res, 0);
	pos = 0;
	res = pomp_buffer_write_fd(buf, &pos, fds[4][0]);
	CU_ASSERT_EQUAL(res, -EBADF);

	/* Cleanup (5th pipe already closed above) */
	pomp_buffer_unref(buf);
	pomp_buffer_unref(buf2);
	for (i = 0; i < 4; i++) {
		res = close(fds[i][0]); CU_ASSERT_EQUAL(res, 0);
		res = close(fds[i][1]); CU_ASSERT_EQUAL(res, 0);
	}
#endif /* !_WIN32 */
}

/** */
static void test_msg_base(void)
{
	int res = 0;
	uint32_t msgid = 0;
	struct pomp_msg *msg = NULL, *msg2 = NULL;

	/* Allocation */
	msg = pomp_msg_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(msg);
	CU_ASSERT_EQUAL(msg->msgid, 0);
	CU_ASSERT_EQUAL(msg->finished, 0);
	CU_ASSERT_PTR_NULL(msg->buf);

	/* Copy (without data) */
	msg2 = pomp_msg_new_copy(msg);
	CU_ASSERT_PTR_NOT_NULL(msg2);
	res = pomp_msg_destroy(msg2);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid copy (NULL param) */
	msg2 = pomp_msg_new_copy(NULL);
	CU_ASSERT_PTR_NULL(msg2);

	/* Invalid get id (NULL param), valid test is in protocol tests */
	msgid = pomp_msg_get_id(NULL);
	CU_ASSERT_EQUAL(msgid, 0);

	/* Destroy */
	res = pomp_msg_destroy(msg);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid destroy (NULL param) */
	res = pomp_msg_destroy(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);
}

/** */
static void test_msg_advanced(void)
{
	int res = 0;
	struct pomp_msg *msg = NULL;
	struct pomp_buffer *buf = NULL;

	/* Allocation */
	msg = pomp_msg_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(msg);

	/* Init with msgid */
	res = pomp_msg_init(msg, TEST_MSGID);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(msg->msgid, TEST_MSGID);

	/* Finish */
	res = pomp_msg_finish(msg);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(msg->finished, 1);
	CU_ASSERT_PTR_NOT_NULL_FATAL(msg->buf);
	CU_ASSERT_TRUE(msg->buf->len >= 12);

	/* Buffer access */
	buf = pomp_msg_get_buffer(msg);
	CU_ASSERT_PTR_NOT_NULL(buf);
	CU_ASSERT_EQUAL(buf, msg->buf);
	buf = pomp_msg_get_buffer(NULL);
	CU_ASSERT_PTR_NULL(buf);

	/* Clear */
	res = pomp_msg_clear(msg);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(msg->msgid, 0);
	CU_ASSERT_EQUAL(msg->finished, 0);
	CU_ASSERT_PTR_NULL(msg->buf);

	/* Double clear */
	res = pomp_msg_clear(msg);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid finish (message cleared or NULL param) */
	res = pomp_msg_finish(msg);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_msg_finish(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid init (NULL param) */
	res = pomp_msg_init(NULL, TEST_MSGID);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid clear (NULL param) */
	res = pomp_msg_clear(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Not permitted init (init called twice) */
	res = pomp_msg_init(msg, TEST_MSGID);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_msg_init(msg, TEST_MSGID);
	CU_ASSERT_EQUAL(res, -EPERM);

	/* Invalid finish (finish called twice) */
	res = pomp_msg_finish(msg);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_msg_finish(msg);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Destroy */
	res = pomp_msg_destroy(msg);
	CU_ASSERT_EQUAL(res, 0);
}

/** */
static void test_msg_read_write(void)
{
	int res = 0;
	struct pomp_msg *msg = NULL, *msg2 = NULL;
	struct test_data dout;
	char buf[512] = "";
	char *abuf;

	/* Allocation */
	msg = pomp_msg_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(msg);

	/* Write */
	res = pomp_msg_write(msg, TEST_MSGID,
			"%hhd%hhu%hd%hu%d%u%"PRId64"%"PRIu64"%s%p%u%f%lf",
			s_refdata.i8, s_refdata.u8,
			s_refdata.i16, s_refdata.u16,
			s_refdata.i32, s_refdata.u32,
			s_refdata.i64, s_refdata.u64,
			s_refdata.cstr,
			s_refdata.cbuf, s_refdata.buflen,
			s_refdata.f32, s_refdata.f64);
	CU_ASSERT_EQUAL(res, 0);

	/* Buffer check */
	CU_ASSERT_EQUAL(msg->buf->len, REFDATA_ENC_SIZE + 12);
	res = memcmp(msg->buf->data, s_refdata_enc_header, 12);
	CU_ASSERT_EQUAL(res, 0);
	res = memcmp(msg->buf->data + 12, s_refdata_enc, REFDATA_ENC_SIZE);
	CU_ASSERT_EQUAL(res, 0);

	/* Read */
	memset(&dout, 0, sizeof(dout));
	res = pomp_msg_read(msg,
			"%hhd%hhu%hd%hu%d%u%"SCNd64"%"SCNu64"%ms%p%u%f%lf",
			&dout.i8, &dout.u8,
			&dout.i16, &dout.u16,
			&dout.i32, &dout.u32,
			&dout.i64, &dout.u64,
			&dout.str,
			&dout.cbuf, &dout.buflen,
			&dout.f32, &dout.f64);
	CU_ASSERT_EQUAL_FATAL(res, 0);

	/* Check */
	CU_ASSERT_EQUAL(dout.i8, TEST_VAL_I8);
	CU_ASSERT_EQUAL(dout.u8, TEST_VAL_U8);
	CU_ASSERT_EQUAL(dout.i16, TEST_VAL_I16);
	CU_ASSERT_EQUAL(dout.u16, TEST_VAL_U16);
	CU_ASSERT_EQUAL(dout.i32, TEST_VAL_I32);
	CU_ASSERT_EQUAL(dout.u32, TEST_VAL_U32);
	CU_ASSERT_EQUAL(dout.i64, TEST_VAL_I64);
	CU_ASSERT_EQUAL(dout.u64, TEST_VAL_U64);
	CU_ASSERT_STRING_EQUAL(dout.str, TEST_VAL_STR);
	CU_ASSERT_EQUAL(dout.buflen, TEST_VAL_BUFLEN);
	CU_ASSERT_EQUAL(memcmp(dout.cbuf, TEST_VAL_BUF, TEST_VAL_BUFLEN), 0);
	CU_ASSERT_EQUAL(dout.f32, TEST_VAL_F32);
	CU_ASSERT_EQUAL(dout.f64, TEST_VAL_F64);
	free(dout.str);

	/* Dump */
	res = pomp_msg_dump(msg, buf, sizeof(buf));
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_STRING_EQUAL(buf, s_msg_dump);

	/* Dump with buffer allocation */
	res = pomp_msg_adump(msg, &abuf);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_STRING_EQUAL(abuf, s_msg_dump);
	free(abuf);

	/* Copy (with data) */
	msg2 = pomp_msg_new_copy(msg);
	CU_ASSERT_PTR_NOT_NULL_FATAL(msg2);

	/* Invalid write (NULL param) */
	res = pomp_msg_write(NULL, TEST_MSGID, "%d", 0);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid read (NULL param) */
	res = pomp_msg_read(NULL, "%d", &dout.i32);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid dump (NULL param) */
	res = pomp_msg_dump(NULL, buf, sizeof(buf));
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_msg_adump(NULL, &abuf);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_msg_adump(msg, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Destroy */
	res = pomp_msg_destroy(msg);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_msg_destroy(msg2);
	CU_ASSERT_EQUAL(res, 0);
}

/** */
static void test_msg_read_write_no_payload(void)
{
	int res = 0;
	struct pomp_msg *msg = NULL;

	/* Allocation */
	msg = pomp_msg_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(msg);

	/* Write */
	res = pomp_msg_write(msg, TEST_MSGID, NULL);
	CU_ASSERT_EQUAL(res, 0);

	/* Buffer check */
	CU_ASSERT_EQUAL(msg->buf->len, 12);
	res = memcmp(msg->buf->data, s_refdata_enc_header_no_payload, 12);
	CU_ASSERT_EQUAL(res, 0);

	/* Read */
	res = pomp_msg_read(msg, NULL);
	CU_ASSERT_EQUAL_FATAL(res, 0);

	/* Destroy */
	res = pomp_msg_destroy(msg);
	CU_ASSERT_EQUAL(res, 0);
}

/** */
static void test_msg_write_argv(void)
{
	int res = 0;
	struct pomp_msg *msg = NULL;
	int argc = 0;
	const char * const *argv = NULL;
	struct test_data dout;

	/* Allocation */
	msg = pomp_msg_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(msg);

	/* argv write */
	argv = get_refdata_argv(&argc);
	res = pomp_msg_write_argv(msg, TEST_MSGID,
			"%hhd%hhu%hd%hu%d%u%"PRId64"%"PRIu64"%s%p%u%f%lf",
			argc, argv);
	CU_ASSERT_EQUAL(res, 0);

	/* Read */
	memset(&dout, 0, sizeof(dout));
	res = pomp_msg_read(msg,
			"%hhd%hhu%hd%hu%d%u%"SCNd64"%"SCNu64"%ms%p%u%f%lf",
			&dout.i8, &dout.u8,
			&dout.i16, &dout.u16,
			&dout.i32, &dout.u32,
			&dout.i64, &dout.u64,
			&dout.str, &dout.cbuf,
			&dout.buflen, &dout.f32,
			&dout.f64);
	CU_ASSERT_EQUAL_FATAL(res, 0);

	/* Check */
	CU_ASSERT_EQUAL(dout.i8, TEST_VAL_I8);
	CU_ASSERT_EQUAL(dout.u8, TEST_VAL_U8);
	CU_ASSERT_EQUAL(dout.i16, TEST_VAL_I16);
	CU_ASSERT_EQUAL(dout.u16, TEST_VAL_U16);
	CU_ASSERT_EQUAL(dout.i32, TEST_VAL_I32);
	CU_ASSERT_EQUAL(dout.u32, TEST_VAL_U32);
	CU_ASSERT_EQUAL(dout.i64, TEST_VAL_I64);
	CU_ASSERT_EQUAL(dout.u64, TEST_VAL_U64);
	CU_ASSERT_STRING_EQUAL(dout.str, TEST_VAL_STR);
	CU_ASSERT_EQUAL(dout.buflen, TEST_VAL_BUFCMDLEN);
	CU_ASSERT_EQUAL(memcmp(dout.cbuf, TEST_VAL_BUF_MEM, TEST_VAL_BUFLEN), 0);
	CU_ASSERT_EQUAL(dout.f32, TEST_VAL_F32);
	CU_ASSERT_EQUAL(dout.f64, TEST_VAL_F64);
	free(dout.str);

	/* Invalid write (NULL param) */
	res = pomp_msg_write_argv(NULL, TEST_MSGID,
			"%hhd%hhu%hd%hu%d%u%"PRId64"%"PRIu64"%s%p%u%f%lf",
			argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Destroy */
	res = pomp_msg_destroy(msg);
	CU_ASSERT_EQUAL(res, 0);
}

/** */
static void test_encoder_base(void)
{
	int res = 0;
	struct pomp_msg msg = POMP_MSG_INITIALIZER;
	struct pomp_encoder *enc = NULL;

	/* Setup message allocated on stack */
	res = pomp_msg_init(&msg, TEST_MSGID);
	CU_ASSERT_EQUAL_FATAL(res, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(msg.buf);

	/* Allocate and init encoder */
	enc = pomp_encoder_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(enc);
	CU_ASSERT_EQUAL(enc->msg, NULL);
	CU_ASSERT_EQUAL(enc->pos, 0);
	res = pomp_encoder_init(enc, &msg);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(enc->msg, &msg);
	CU_ASSERT_EQUAL(enc->pos, 12);

	/* Write */
	res = pomp_encoder_write_i8(enc, TEST_VAL_I8);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_encoder_write_u8(enc, TEST_VAL_U8);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_encoder_write_i16(enc, TEST_VAL_I16);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_encoder_write_u16(enc, TEST_VAL_U16);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_encoder_write_i32(enc, TEST_VAL_I32);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_encoder_write_u32(enc, TEST_VAL_U32);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_encoder_write_i64(enc, TEST_VAL_I64);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_encoder_write_u64(enc, TEST_VAL_U64);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_encoder_write_str(enc, TEST_VAL_STR);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_encoder_write_buf(enc, TEST_VAL_BUF, TEST_VAL_BUFLEN);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_encoder_write_f32(enc, TEST_VAL_F32);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_encoder_write_f64(enc, TEST_VAL_F64);
	CU_ASSERT_EQUAL(res, 0);

	/* Buffer check */
	CU_ASSERT_EQUAL(enc->msg->buf->len, REFDATA_ENC_SIZE + 12);
	res = memcmp(enc->msg->buf->data + 12, s_refdata_enc, REFDATA_ENC_SIZE);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid write (NULL param) */
	res = pomp_encoder_write_i8(NULL, TEST_VAL_I8);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_u8(NULL, TEST_VAL_U8);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_i16(NULL, TEST_VAL_I16);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_u16(NULL, TEST_VAL_U16);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_i32(NULL, TEST_VAL_I32);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_u32(NULL, TEST_VAL_U32);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_i64(NULL, TEST_VAL_I64);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_u64(NULL, TEST_VAL_U64);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_str(NULL, TEST_VAL_STR);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_str(enc, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_buf(NULL, TEST_VAL_BUF, TEST_VAL_BUFLEN);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_buf(enc, NULL, 0);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_f32(NULL, TEST_VAL_F32);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_f64(NULL, TEST_VAL_F64);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid write (string too long) */
	{
		char *longstr = malloc(0x10000);
		memset(longstr, 'a', 0xffff);
		longstr[0xffff] = '\0';
		res = pomp_encoder_write_str(enc, longstr);
		CU_ASSERT_EQUAL(res, -EINVAL);
		free(longstr);
	}

	/* Invalid write (not permitted) */
	res = pomp_msg_finish(&msg);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_encoder_write_i8(enc, TEST_VAL_I8);
	CU_ASSERT_EQUAL(res, -EPERM);
	res = pomp_encoder_write_u8(enc, TEST_VAL_U8);
	CU_ASSERT_EQUAL(res, -EPERM);
	res = pomp_encoder_write_i16(enc, TEST_VAL_I16);
	CU_ASSERT_EQUAL(res, -EPERM);
	res = pomp_encoder_write_u16(enc, TEST_VAL_U16);
	CU_ASSERT_EQUAL(res, -EPERM);
	res = pomp_encoder_write_i32(enc, TEST_VAL_I32);
	CU_ASSERT_EQUAL(res, -EPERM);
	res = pomp_encoder_write_u32(enc, TEST_VAL_U32);
	CU_ASSERT_EQUAL(res, -EPERM);
	res = pomp_encoder_write_i64(enc, TEST_VAL_I64);
	CU_ASSERT_EQUAL(res, -EPERM);
	res = pomp_encoder_write_u64(enc, TEST_VAL_U64);
	CU_ASSERT_EQUAL(res, -EPERM);
	res = pomp_encoder_write_str(enc, TEST_VAL_STR);
	CU_ASSERT_EQUAL(res, -EPERM);
	res = pomp_encoder_write_buf(enc, TEST_VAL_BUF, TEST_VAL_BUFLEN);
	CU_ASSERT_EQUAL(res, -EPERM);
	res = pomp_encoder_write_f32(enc, TEST_VAL_F32);
	CU_ASSERT_EQUAL(res, -EPERM);
	res = pomp_encoder_write_f64(enc, TEST_VAL_F64);
	CU_ASSERT_EQUAL(res, -EPERM);

	/* Clear */
	res = pomp_encoder_clear(enc);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid clear (NULL param) */
	res = pomp_encoder_clear(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid write (cleared encoder) */
	res = pomp_encoder_write_i8(enc, TEST_VAL_I8);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_u8(enc, TEST_VAL_U8);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_i16(enc, TEST_VAL_I16);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_u16(enc, TEST_VAL_U16);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_i32(enc, TEST_VAL_I32);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_u32(enc, TEST_VAL_U32);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_i64(enc, TEST_VAL_I64);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_u64(enc, TEST_VAL_U64);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_str(enc, TEST_VAL_STR);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_buf(enc, TEST_VAL_BUF, TEST_VAL_BUFLEN);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_f32(enc, TEST_VAL_F32);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_f64(enc, TEST_VAL_F64);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Destroy */
	res = pomp_encoder_destroy(enc);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid init (NULL param) */
	res = pomp_encoder_init(NULL, &msg);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_init(enc, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid destroy (NULL param) */
	res = pomp_encoder_destroy(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Clear message allocated on stack */
	res = pomp_msg_clear(&msg);
	CU_ASSERT_EQUAL(res, 0);
}

/** */
static void test_encoder_printf(void)
{
	int res = 0;
	struct pomp_msg msg = POMP_MSG_INITIALIZER;
	struct pomp_encoder *enc = NULL;
	const char *nullstr = NULL;
	const void *nullbuf = NULL;
	long double longdouble = 0.0;

	/* Setup message allocated on stack */
	res = pomp_msg_init(&msg, TEST_MSGID);
	CU_ASSERT_EQUAL_FATAL(res, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(msg.buf);

	/* Allocate and init encoder */
	enc = pomp_encoder_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(enc);
	res = pomp_encoder_init(enc, &msg);
	CU_ASSERT_EQUAL(res, 0);

	/* Write */
	res = pomp_encoder_write(enc,
			"%hhd%hhu%hd%hu%d%u%"PRId64"%"PRIu64"%s%p%u%f%lf",
			s_refdata.i8, s_refdata.u8,
			s_refdata.i16, s_refdata.u16,
			s_refdata.i32, s_refdata.u32,
			s_refdata.i64, s_refdata.u64,
			s_refdata.cstr,
			s_refdata.cbuf, s_refdata.buflen,
			s_refdata.f32, s_refdata.f64);
	CU_ASSERT_EQUAL(res, 0);

	/* Buffer check */
	CU_ASSERT_EQUAL(enc->msg->buf->len, REFDATA_ENC_SIZE + 12);
	res = memcmp(enc->msg->buf->data + 12, s_refdata_enc, REFDATA_ENC_SIZE);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid write (NULL param) */
	res = pomp_encoder_write(NULL, "%d", 0);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid write (invalid format char) */
	res = pomp_encoder_write(enc, "K");
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid write (invalid format specifier) */
	res = pomp_encoder_write(enc, "%o", 0);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid write (invalid format width) */
	res = pomp_encoder_write(enc, "%llf", longdouble);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid write (missing %u after %p) */
	res = pomp_encoder_write(enc, "%p", NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid write (missing %u after %p) */
	res = pomp_encoder_write(enc, "%p%i", NULL, 0);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid write (NULL string) */
	res = pomp_encoder_write(enc, "%s", nullstr);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid write (NULL buffer) */
	res = pomp_encoder_write(enc, "%p%u", nullbuf, 0);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Destroy */
	res = pomp_encoder_destroy(enc);
	CU_ASSERT_EQUAL(res, 0);

	/* Clear message allocated on stack */
	res = pomp_msg_clear(&msg);
	CU_ASSERT_EQUAL(res, 0);
}

/** */
static void test_encoder_printf_no_payload(void)
{
	int res = 0;
	struct pomp_msg msg = POMP_MSG_INITIALIZER;
	struct pomp_encoder *enc = NULL;

	/* Setup message allocated on stack */
	res = pomp_msg_init(&msg, TEST_MSGID);
	CU_ASSERT_EQUAL_FATAL(res, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(msg.buf);

	/* Allocate and init encoder */
	enc = pomp_encoder_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(enc);
	res = pomp_encoder_init(enc, &msg);
	CU_ASSERT_EQUAL(res, 0);

	/* Write */
	res = pomp_encoder_write(enc, NULL);
	CU_ASSERT_EQUAL(res, 0);

	/* Destroy */
	res = pomp_encoder_destroy(enc);
	CU_ASSERT_EQUAL(res, 0);

	/* Clear message allocated on stack */
	res = pomp_msg_clear(&msg);
	CU_ASSERT_EQUAL(res, 0);
}

/** */
static void test_encoder_printf_32_64(void)
{
	int res = 0;
	struct pomp_msg msg = POMP_MSG_INITIALIZER;
	struct pomp_encoder *enc = NULL;

	/* Setup message allocated on stack */
	res = pomp_msg_init(&msg, TEST_MSGID);
	CU_ASSERT_EQUAL_FATAL(res, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(msg.buf);

	/* Allocate and init encoder */
	enc = pomp_encoder_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(enc);
	res = pomp_encoder_init(enc, &msg);
	CU_ASSERT_EQUAL(res, 0);

#if __WORDSIZE == 64
	{
		/* with wordsize == 64, %l and %ll are 64 bits, test with
		 * %lld and %llu as PRId64/PRIu64 are defined with %ld/%lu */
		long long lld = s_refdata.i64;
		unsigned long long llu = s_refdata.u64;
		res = pomp_encoder_write(enc,
				"%hhd%hhu%hd%hu%d%u%lld%llu%s%p%u%f%lf",
				s_refdata.i8, s_refdata.u8,
				s_refdata.i16, s_refdata.u16,
				s_refdata.i32, s_refdata.u32,
				lld, llu,
				s_refdata.cstr,
				s_refdata.cbuf, s_refdata.buflen,
				s_refdata.f32, s_refdata.f64);
		CU_ASSERT_EQUAL(res, 0);
	}
#else
	{
		/* with wordsize == 32, %l is 32 bits */
		long ld = s_refdata.i32;
		unsigned long lu = s_refdata.u32;
		res = pomp_encoder_write(enc,
				"%hhd%hhu%hd%hu%ld%lu%"PRId64"%"PRIu64"%s%p%u%f%lf",
				s_refdata.i8, s_refdata.u8,
				s_refdata.i16, s_refdata.u16,
				ld, lu,
				s_refdata.i64, s_refdata.u64,
				s_refdata.cstr,
				s_refdata.cbuf, s_refdata.buflen,
				s_refdata.f32, s_refdata.f64);
		CU_ASSERT_EQUAL(res, 0);
	}
#endif

	/* Buffer check */
	CU_ASSERT_EQUAL(enc->msg->buf->len, REFDATA_ENC_SIZE + 12);
	res = memcmp(enc->msg->buf->data + 12, s_refdata_enc, REFDATA_ENC_SIZE);
	CU_ASSERT_EQUAL(res, 0);

	/* Destroy */
	res = pomp_encoder_destroy(enc);
	CU_ASSERT_EQUAL(res, 0);

	/* Clear message allocated on stack */
	res = pomp_msg_clear(&msg);
	CU_ASSERT_EQUAL(res, 0);
}

/** */
static void test_encoder_argv(void)
{
	int res = 0;
	struct pomp_msg msg = POMP_MSG_INITIALIZER;
	struct pomp_encoder *enc = NULL;
	int argc = 0;
	const char **argv = NULL;

	/* Setup message allocated on stack */
	res = pomp_msg_init(&msg, TEST_MSGID);
	CU_ASSERT_EQUAL_FATAL(res, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(msg.buf);

	/* Allocate and init encoder */
	enc = pomp_encoder_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(enc);
	res = pomp_encoder_init(enc, &msg);
	CU_ASSERT_EQUAL(res, 0);

	/* argv write */
	argv = get_refdata_argv(&argc);
	res = pomp_encoder_write_argv(enc,
			"%hhd%hhu%hd%hu%d%u%"PRId64"%"PRIu64"%s%p%u%f%lf",
			argc, argv);
	CU_ASSERT_EQUAL(res, 0);

	/* Buffer check */
	CU_ASSERT_EQUAL(enc->msg->buf->len, REFDATA_ENC_ARGV_SIZE + 12);
	res = memcmp(enc->msg->buf->data + 12, s_refdata_enc_argv, REFDATA_ENC_ARGV_SIZE);
	CU_ASSERT_EQUAL(res, 0);

	res = pomp_encoder_init(enc, &msg);
	CU_ASSERT_EQUAL(res, 0);
#if __WORDSIZE == 64
	/* with wordsize == 64, %l and %ll are 64 bits, test with
	 * %lld and %llu as PRId64/PRIu64 are defined with %ld/%lu */
	{
		const char *argv2[] = {"1234", "5678"};
		res = pomp_encoder_write_argv(enc, "%lld%llu", 2, argv2);
		CU_ASSERT_EQUAL(res, 0);
	}
#else
	/* with wordsize == 32, %l is 32 bits */
	{
		const char *argv2[] = {"1234", "5678"};
		res = pomp_encoder_write_argv(enc, "%ld%lu", 2, argv2);
		CU_ASSERT_EQUAL(res, 0);
	}
#endif

	/* Empty write */
	res = pomp_encoder_init(enc, &msg);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_encoder_write_argv(enc, NULL, 0, NULL);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid write (NULL param) */
	res = pomp_encoder_init(enc, &msg);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_encoder_write_argv(enc,
			"%hhd%hhu%hd%hu%d%u%"PRId64"%"PRIu64"%s%p%u%f%lf",
			argc, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid write (missing arg) */
	argc = 1;
	res = pomp_encoder_write_argv(enc, "%hhd%hhd", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_argv(enc, "%hhd%hd", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_argv(enc, "%hhd%d", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_argv(enc, "%hhd%ld", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_argv(enc, "%hhd%lld", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_argv(enc, "%hhd%hhu", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_argv(enc, "%hhd%hu", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_argv(enc, "%hhd%u", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_argv(enc, "%hhd%lu", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_argv(enc, "%hhd%llu", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_argv(enc, "%hhd%s", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_argv(enc, "%hhd%f", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_argv(enc, "%hhd%lf", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_argv(enc, "%p", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);


	/* Invalid write (NULL arg) */
	argc = 2;
	argv[1] = NULL;
	res = pomp_encoder_write_argv(enc, "%hhd%hhd", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_argv(enc, "%hhd%hd", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_argv(enc, "%hhd%d", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_argv(enc, "%hhd%ld", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_argv(enc, "%hhd%lld", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_argv(enc, "%hhd%hhu", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_argv(enc, "%hhd%hu", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_argv(enc, "%hhd%u", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_argv(enc, "%hhd%lu", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_argv(enc, "%hhd%llu", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_argv(enc, "%hhd%s", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_argv(enc, "%hhd%f", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_argv(enc, "%hhd%lf", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_encoder_write_argv(enc, "%p%u", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);


	/* Invalid write (Not supported) */
	argc = 2;
	argv[1] = NULL;
	res = pomp_encoder_write_argv(enc, "%p%d", argc, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Destroy */
	res = pomp_encoder_destroy(enc);
	CU_ASSERT_EQUAL(res, 0);

	/* Clear message allocated on stack */
	res = pomp_msg_clear(&msg);
	CU_ASSERT_EQUAL(res, 0);
}

/** */
static void test_encoder_fd(void)
{
#ifndef _WIN32
	int res = 0;
	struct pomp_msg msg = POMP_MSG_INITIALIZER;
	struct pomp_encoder *enc = NULL;
	int fds[2] = {-1, -1};
	const char *argv[2] = {"0", "0"};

	/* Setup message allocated on stack */
	res = pomp_msg_init(&msg, TEST_MSGID);
	CU_ASSERT_EQUAL_FATAL(res, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(msg.buf);

	/* Allocate and init encoder */
	enc = pomp_encoder_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(enc);
	CU_ASSERT_EQUAL(enc->msg, NULL);
	CU_ASSERT_EQUAL(enc->pos, 0);
	res = pomp_encoder_init(enc, &msg);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(enc->msg, &msg);
	CU_ASSERT_EQUAL(enc->pos, 12);

	/* Create file descriptors */
	res = pipe(fds);
	CU_ASSERT_EQUAL(res, 0);

	/* Write */
	res = pomp_encoder_write_fd(enc, fds[0]);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_encoder_write_argv(enc, "%hhd%x", 2, argv);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid write (NULL param) */
	res = pomp_encoder_write_fd(NULL, fds[0]);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid argv write (missing arg) */
	res = pomp_encoder_write_argv(enc, "%hhd%x", 1, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid argv write (NULL arg) */
	argv[1] = NULL;
	res = pomp_encoder_write_argv(enc, "%hhd%x", 2, argv);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid write (invalid format width) */
	res = pomp_encoder_write(enc, "%lx", 0ul);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid write (not permitted) */
	res = pomp_msg_finish(&msg);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_encoder_write_fd(enc, fds[0]);
	CU_ASSERT_EQUAL(res, -EPERM);

	/* Clear */
	res = pomp_encoder_clear(enc);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid write (cleared encoder) */
	res = pomp_encoder_write_fd(enc, fds[0]);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Close pipes */
	res = close(fds[0]); CU_ASSERT_EQUAL(res, 0);
	res = close(fds[1]); CU_ASSERT_EQUAL(res, 0);

	/* Destroy */
	res = pomp_encoder_destroy(enc);
	CU_ASSERT_EQUAL(res, 0);

	/* Clear message allocated on stack */
	res = pomp_msg_clear(&msg);
	CU_ASSERT_EQUAL(res, 0);
#endif /* !_WIN32 */
}

/** */
static void test_decoder_base(void)
{
	int res = 0;
	struct pomp_msg msg = POMP_MSG_INITIALIZER;
	struct pomp_decoder *dec = NULL;
	size_t pos = 0;
	struct test_data dout;

	/* Setup message allocated on stack */
	res = pomp_msg_init(&msg, TEST_MSGID);
	CU_ASSERT_EQUAL_FATAL(res, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(msg.buf);

	/* Allocate and init decoder */
	dec = pomp_decoder_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(dec);
	CU_ASSERT_EQUAL(dec->msg, NULL);
	CU_ASSERT_EQUAL(dec->pos, 0);
	res = pomp_decoder_init(dec, &msg);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(dec->msg, &msg);
	CU_ASSERT_EQUAL(dec->pos, 12);

	/* Setup buffer */
	pos = 12;
	res = pomp_buffer_write(msg.buf, &pos, s_refdata_enc, REFDATA_ENC_SIZE);
	CU_ASSERT_EQUAL(res, 0);

	/* Read */
	memset(&dout, 0, sizeof(dout));
	res = pomp_decoder_read_i8(dec, &dout.i8);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_read_u8(dec, &dout.u8);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_read_i16(dec, &dout.i16);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_read_u16(dec, &dout.u16);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_read_i32(dec, &dout.i32);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_read_u32(dec, &dout.u32);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_read_i64(dec, &dout.i64);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_read_u64(dec, &dout.u64);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_read_str(dec, &dout.str);
	CU_ASSERT_EQUAL_FATAL(res, 0);
	res = pomp_decoder_read_buf(dec, &dout.buf, &dout.buflen);
	CU_ASSERT_EQUAL_FATAL(res, 0);
	res = pomp_decoder_read_f32(dec, &dout.f32);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_read_f64(dec, &dout.f64);
	CU_ASSERT_EQUAL(res, 0);

	/* Check */
	CU_ASSERT_EQUAL(dout.i8, TEST_VAL_I8);
	CU_ASSERT_EQUAL(dout.u8, TEST_VAL_U8);
	CU_ASSERT_EQUAL(dout.i16, TEST_VAL_I16);
	CU_ASSERT_EQUAL(dout.u16, TEST_VAL_U16);
	CU_ASSERT_EQUAL(dout.i32, TEST_VAL_I32);
	CU_ASSERT_EQUAL(dout.u32, TEST_VAL_U32);
	CU_ASSERT_EQUAL(dout.i64, TEST_VAL_I64);
	CU_ASSERT_EQUAL(dout.u64, TEST_VAL_U64);
	CU_ASSERT_STRING_EQUAL(dout.str, TEST_VAL_STR);
	CU_ASSERT_EQUAL(dout.buflen, TEST_VAL_BUFLEN);
	CU_ASSERT_EQUAL(memcmp(dout.buf, TEST_VAL_BUF, TEST_VAL_BUFLEN), 0);
	CU_ASSERT_EQUAL(dout.f32, TEST_VAL_F32);
	CU_ASSERT_EQUAL(dout.f64, TEST_VAL_F64);
	free(dout.str);
	free(dout.buf);

	/* Invalid read (end of buffer) */
	res = pomp_decoder_read_i8(dec, &dout.i8);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_i32(dec, &dout.i32);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_cstr(dec, &dout.cstr);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_cbuf(dec, &dout.cbuf, &dout.buflen);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid read (NULL param) */
	res = pomp_decoder_read_i8(NULL, &dout.i8);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_i8(dec, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_u8(NULL, &dout.u8);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_u8(dec, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_i16(NULL, &dout.i16);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_i16(dec, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_u16(NULL, &dout.u16);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_u16(dec, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_i32(NULL, &dout.i32);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_i32(dec, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_u32(NULL, &dout.u32);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_u32(dec, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_i64(NULL, &dout.i64);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_i64(dec, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_u64(NULL, &dout.u64);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_u64(dec, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	res = pomp_decoder_read_str(NULL, &dout.str);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_str(dec, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	res = pomp_decoder_read_cstr(NULL, &dout.cstr);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_cstr(dec, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	res = pomp_decoder_read_buf(NULL, &dout.buf, &dout.buflen);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_buf(dec, NULL, &dout.buflen);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_buf(dec, &dout.buf, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_buf(dec, NULL, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	res = pomp_decoder_read_cbuf(NULL, &dout.cbuf, &dout.buflen);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_cbuf(dec, NULL, &dout.buflen);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_cbuf(dec, &dout.cbuf, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_cbuf(dec, NULL, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	res = pomp_decoder_read_f32(NULL, &dout.f32);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_f32(dec, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_f64(NULL, &dout.f64);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_f64(dec, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid read (wrong type) */
	res = pomp_decoder_init(dec, &msg);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_read_u8(dec, &dout.u8);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_i16(dec, &dout.i16);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_u16(dec, &dout.u16);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_i32(dec, &dout.i32);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_u32(dec, &dout.u32);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_i64(dec, &dout.i64);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_u64(dec, &dout.u64);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_str(dec, &dout.str);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_cstr(dec, &dout.cstr);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_buf(dec, &dout.buf, &dout.buflen);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_cbuf(dec, &dout.cbuf, &dout.buflen);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_f32(dec, &dout.f32);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_f64(dec, &dout.f64);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid read (wrong type) */
	res = pomp_decoder_read_i8(dec, &dout.i8);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_read_i8(dec, &dout.i8);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Clear */
	res = pomp_decoder_clear(dec);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid clear (NULL param) */
	res = pomp_decoder_clear(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid read (cleared decoder) */
	res = pomp_decoder_read_i8(dec, &dout.i8);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_u8(dec, &dout.u8);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_i16(dec, &dout.i16);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_u16(dec, &dout.u16);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_i32(dec, &dout.i32);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_u32(dec, &dout.u32);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_i64(dec, &dout.i64);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_u64(dec, &dout.u64);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_str(dec, &dout.str);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_cstr(dec, &dout.cstr);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_buf(dec, &dout.buf, &dout.buflen);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_cbuf(dec, &dout.cbuf, &dout.buflen);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_f32(dec, &dout.f32);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_f64(dec, &dout.f64);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Destroy */
	res = pomp_decoder_destroy(dec);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid init (NULL param) */
	res = pomp_decoder_init(NULL, &msg);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_init(dec, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid destroy (NULL param) */
	res = pomp_decoder_destroy(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Clear message allocated on stack */
	res = pomp_msg_clear(&msg);
	CU_ASSERT_EQUAL(res, 0);
}

/** */
static void test_decoder_partial(void)
{
	int res = 0;
	struct pomp_msg msg = POMP_MSG_INITIALIZER;
	struct pomp_decoder *dec = NULL;
	size_t pos = 0;
	struct test_data dout;
	uint8_t varintlen = 0;

	/* Setup message allocated on stack */
	res = pomp_msg_init(&msg, TEST_MSGID);
	CU_ASSERT_EQUAL_FATAL(res, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(msg.buf);

	/* Allocate and init decoder */
	dec = pomp_decoder_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(dec);
	res = pomp_decoder_init(dec, &msg);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid i8 read (partial buffer) */
	{
		pomp_buffer_clear(msg.buf);

		/* Write i8 type */
		pos = 12;
		res = pomp_buffer_writeb(msg.buf, &pos, 0x01);
		CU_ASSERT_EQUAL(res, 0);

		/* Read i8, not enough data  */
		res = pomp_decoder_init(dec, &msg);
		CU_ASSERT_EQUAL(res, 0);
		res = pomp_decoder_read_i8(dec, &dout.i8);
		CU_ASSERT_EQUAL(res, -EINVAL);
	}

	/* Invalid i32 read (partial buffer) */
	{
		pomp_buffer_clear(msg.buf);

		/* Write i32 type */
		pos = 12;
		res = pomp_buffer_writeb(msg.buf, &pos, 0x05);
		CU_ASSERT_EQUAL(res, 0);

		/* Read i32, not enough data  */
		res = pomp_decoder_init(dec, &msg);
		CU_ASSERT_EQUAL(res, 0);
		res = pomp_decoder_read_i32(dec, &dout.i32);
		CU_ASSERT_EQUAL(res, -EINVAL);
	}

	/* Invalid str read (partial buffer) */
	{
		pomp_buffer_clear(msg.buf);

		/* Write string type */
		pos = 12;
		res = pomp_buffer_writeb(msg.buf, &pos, 0x09);
		CU_ASSERT_EQUAL(res, 0);

		/* Read str, missing length */
		res = pomp_decoder_init(dec, &msg);
		CU_ASSERT_EQUAL(res, 0);
		res = pomp_decoder_read_cstr(dec, &dout.cstr);
		CU_ASSERT_EQUAL(res, -EINVAL);

		/* Write 0 length */
		pos = 13;
		varintlen = 0;
		res = pomp_buffer_write(msg.buf, &pos, &varintlen, 1);
		CU_ASSERT_EQUAL(res, 0);

		/* Read str, invalid length */
		res = pomp_decoder_init(dec, &msg);
		CU_ASSERT_EQUAL(res, 0);
		res = pomp_decoder_read_cstr(dec, &dout.cstr);
		CU_ASSERT_EQUAL(res, -EINVAL);

		/* Write 5 length and only 4 data bytes*/
		pos = 13;
		varintlen = 5;
		res = pomp_buffer_write(msg.buf, &pos, &varintlen, 1);
		CU_ASSERT_EQUAL(res, 0);
		res = pomp_buffer_write(msg.buf, &pos, "abcd", 4);
		CU_ASSERT_EQUAL(res, 0);

		/* Read str, not enough data */
		res = pomp_decoder_init(dec, &msg);
		CU_ASSERT_EQUAL(res, 0);
		res = pomp_decoder_read_cstr(dec, &dout.cstr);
		CU_ASSERT_EQUAL(res, -EINVAL);

		/* Write 5th data, not 0 */
		res = pomp_buffer_write(msg.buf, &pos, "e", 1);
		CU_ASSERT_EQUAL(res, 0);

		/* Read str, not null terminated */
		res = pomp_decoder_init(dec, &msg);
		CU_ASSERT_EQUAL(res, 0);
		res = pomp_decoder_read_cstr(dec, &dout.cstr);
		CU_ASSERT_EQUAL(res, -EINVAL);
	}

	/* Invalid buf read (partial buffer) */
	{
		pomp_buffer_clear(msg.buf);

		/* Write buffer type */
		pos = 12;
		res = pomp_buffer_writeb(msg.buf, &pos, 0x0a);
		CU_ASSERT_EQUAL(res, 0);

		/* Read buf, missing length */
		res = pomp_decoder_init(dec, &msg);
		CU_ASSERT_EQUAL(res, 0);
		res = pomp_decoder_read_cbuf(dec, &dout.cbuf, &dout.buflen);
		CU_ASSERT_EQUAL(res, -EINVAL);

		/* Write 5 length and only 4 data bytes*/
		pos = 13;
		varintlen = 5;
		res = pomp_buffer_write(msg.buf, &pos, &varintlen, 1);
		CU_ASSERT_EQUAL(res, 0);
		res = pomp_buffer_write(msg.buf, &pos, "abcd", 4);
		CU_ASSERT_EQUAL(res, 0);

		/* Read buf, not enough data */
		res = pomp_decoder_init(dec, &msg);
		CU_ASSERT_EQUAL(res, 0);
		res = pomp_decoder_read_cbuf(dec, &dout.cbuf, &dout.buflen);
		CU_ASSERT_EQUAL(res, -EINVAL);
	}

	/* Destroy */
	res = pomp_decoder_destroy(dec);
	CU_ASSERT_EQUAL(res, 0);

	/* Clear message allocated on stack */
	res = pomp_msg_clear(&msg);
	CU_ASSERT_EQUAL(res, 0);
}

/** */
static void test_decoder_scanf(void)
{
	int res = 0;
	struct pomp_msg msg = POMP_MSG_INITIALIZER;
	struct pomp_msg msg2 = POMP_MSG_INITIALIZER;
	struct pomp_decoder *dec = NULL;
	size_t pos = 0;
	struct test_data dout;
	int octout = 0;
	long double longdouble = 0.0;
	char strout[32];
	int intout = 0;
	char *strarray[17];

	/* Setup message allocated on stack */
	res = pomp_msg_init(&msg, TEST_MSGID);
	CU_ASSERT_EQUAL_FATAL(res, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(msg.buf);

	/* Allocate and init decoder */
	dec = pomp_decoder_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(dec);
	res = pomp_decoder_init(dec, &msg);
	CU_ASSERT_EQUAL(res, 0);

	/* Setup buffer */
	pos = 12;
	res = pomp_buffer_write(msg.buf, &pos, s_refdata_enc, REFDATA_ENC_SIZE);
	CU_ASSERT_EQUAL(res, 0);

	/* Read */
	memset(&dout, 0, sizeof(dout));
	res = pomp_decoder_read(dec,
			"%hhd%hhu%hd%hu%d%u%"SCNd64"%"SCNu64"%ms%p%u%f%lf",
			&dout.i8, &dout.u8,
			&dout.i16, &dout.u16,
			&dout.i32, &dout.u32,
			&dout.i64, &dout.u64,
			&dout.str,
			&dout.cbuf, &dout.buflen,
			&dout.f32, &dout.f64);
	CU_ASSERT_EQUAL_FATAL(res, 0);

	/* Check */
	CU_ASSERT_EQUAL(dout.i8, TEST_VAL_I8);
	CU_ASSERT_EQUAL(dout.u8, TEST_VAL_U8);
	CU_ASSERT_EQUAL(dout.i16, TEST_VAL_I16);
	CU_ASSERT_EQUAL(dout.u16, TEST_VAL_U16);
	CU_ASSERT_EQUAL(dout.i32, TEST_VAL_I32);
	CU_ASSERT_EQUAL(dout.u32, TEST_VAL_U32);
	CU_ASSERT_EQUAL(dout.i64, TEST_VAL_I64);
	CU_ASSERT_EQUAL(dout.u64, TEST_VAL_U64);
	CU_ASSERT_STRING_EQUAL(dout.str, TEST_VAL_STR);
	CU_ASSERT_EQUAL(dout.buflen, TEST_VAL_BUFLEN);
	CU_ASSERT_EQUAL(memcmp(dout.cbuf, TEST_VAL_BUF, TEST_VAL_BUFLEN), 0);
	CU_ASSERT_EQUAL(dout.f32, TEST_VAL_F32);
	CU_ASSERT_EQUAL(dout.f64, TEST_VAL_F64);
	free(dout.str);

	/* Invalid read (NULL param) */
	res = pomp_decoder_read(NULL, "%d", &dout.i32);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid read (invalid format char) */
	res = pomp_decoder_read(dec, "K");
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid read (invalid format specifier) */
	res = pomp_decoder_read(dec, "%o", &octout);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid read (invalid string specifier) */
	res = pomp_decoder_read(dec, "%s", strout);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid write (invalid format width) */
	res = pomp_decoder_read(dec, "%llf", &longdouble);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid read (missing %u after %p) */
	res = pomp_decoder_read(dec, "%p", &dout.cbuf);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid read (missing %u after %p) */
	res = pomp_decoder_read(dec, "%p%i", &dout.cbuf, &intout);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid read (too many fields) with automatic free of already
	 * decoded strings */
	memset(&dout, 0, sizeof(dout));
	res = pomp_decoder_init(dec, &msg);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_read(dec,
			"%hhd%hhu%hd%hu%d%u%"SCNd64"%"SCNu64"%ms%d%f%lf",
			&dout.i8, &dout.u8,
			&dout.i16, &dout.u16,
			&dout.i32, &dout.u32,
			&dout.i64, &dout.u64,
			&dout.str, &intout,
			&dout.f32, &dout.f64);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid read (too many strings) */
	res = pomp_msg_write(&msg2, TEST_MSGID,
			"%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
			TEST_VAL_STR, TEST_VAL_STR, TEST_VAL_STR, TEST_VAL_STR,
			TEST_VAL_STR, TEST_VAL_STR, TEST_VAL_STR, TEST_VAL_STR,
			TEST_VAL_STR, TEST_VAL_STR, TEST_VAL_STR, TEST_VAL_STR,
			TEST_VAL_STR, TEST_VAL_STR, TEST_VAL_STR, TEST_VAL_STR,
			TEST_VAL_STR);
	res = pomp_decoder_init(dec, &msg2);
	CU_ASSERT_EQUAL(res, 0);
	memset(strarray, 0, sizeof(strarray));
	res = pomp_decoder_read(dec, "%ms%ms%ms%ms%ms%ms%ms%ms"
			"%ms%ms%ms%ms%ms%ms%ms%ms%ms",
			&strarray[0], &strarray[1], &strarray[2], &strarray[3],
			&strarray[4], &strarray[5], &strarray[6], &strarray[7],
			&strarray[8], &strarray[9], &strarray[10],
			&strarray[11], &strarray[12], &strarray[13],
			&strarray[14], &strarray[15], &strarray[16]);
	CU_ASSERT_EQUAL(res, -E2BIG);

	/* Invalid read (wrong type) */
	res = pomp_decoder_init(dec, &msg);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_read(dec, "%hhu", &dout.u8);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read(dec, "%hd", &dout.i16);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read(dec, "%hu", &dout.u16);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read(dec, "%d", &dout.i32);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read(dec, "%u", &dout.u32);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read(dec, "%" SCNd64, &dout.i64);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read(dec, "%" SCNu64, &dout.u64);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read(dec, "%ms", &dout.str);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read(dec, "%p%u", &dout.cbuf, &dout.buflen);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read(dec, "%f", &dout.f32);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read(dec, "%lf", &dout.f64);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid read (wrong type) */
	res = pomp_decoder_read(dec, "%hhd", &dout.i8);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_read(dec, "%hhd", &dout.i8);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Destroy */
	res = pomp_decoder_destroy(dec);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid destroy (NULL param) */
	res = pomp_decoder_destroy(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Clear message allocated on stack */
	res = pomp_msg_clear(&msg);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_msg_clear(&msg2);
	CU_ASSERT_EQUAL(res, 0);
}

/** */
static void test_decoder_scanf_no_payload(void)
{
	int res = 0;
	struct pomp_msg msg = POMP_MSG_INITIALIZER;
	struct pomp_decoder *dec = NULL;

	/* Setup message allocated on stack */
	res = pomp_msg_init(&msg, TEST_MSGID);
	CU_ASSERT_EQUAL_FATAL(res, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(msg.buf);

	/* Allocate and init decoder */
	dec = pomp_decoder_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(dec);
	res = pomp_decoder_init(dec, &msg);
	CU_ASSERT_EQUAL(res, 0);

	/* Read */
	res = pomp_decoder_read(dec, NULL);
	CU_ASSERT_EQUAL_FATAL(res, 0);

	/* Destroy */
	res = pomp_decoder_destroy(dec);
	CU_ASSERT_EQUAL(res, 0);

	/* Clear message allocated on stack */
	res = pomp_msg_clear(&msg);
	CU_ASSERT_EQUAL(res, 0);
}

/** */
static void test_decoder_scanf_32_64(void)
{
	int res = 0;
	struct pomp_msg msg = POMP_MSG_INITIALIZER;
	struct pomp_decoder *dec = NULL;
	size_t pos = 0;
	struct test_data dout;

	/* Setup message allocated on stack */
	res = pomp_msg_init(&msg, TEST_MSGID);
	CU_ASSERT_EQUAL_FATAL(res, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(msg.buf);

	/* Allocate and init decoder */
	dec = pomp_decoder_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(dec);
	res = pomp_decoder_init(dec, &msg);
	CU_ASSERT_EQUAL(res, 0);

	/* Setup buffer */
	pos = 12;
	res = pomp_buffer_write(msg.buf, &pos, s_refdata_enc, REFDATA_ENC_SIZE);
	CU_ASSERT_EQUAL(res, 0);

#if __WORDSIZE == 64
	{
		/* with wordsize == 64, %l and %ll are 64 bits, test with
		 * %lld and %llu as SCNd64/SCNu64 are defined with %ld/%lu */
		long long lld = 0;
		unsigned long long llu = 0;
		memset(&dout, 0, sizeof(dout));
		res = pomp_decoder_read(dec,
				"%hhd%hhu%hd%hu%d%u%lld%llu%ms%p%u%f%lf",
				&dout.i8, &dout.u8,
				&dout.i16, &dout.u16,
				&dout.i32, &dout.u32,
				&lld, &llu,
				&dout.str,
				&dout.cbuf, &dout.buflen,
				&dout.f32, &dout.f64);
		CU_ASSERT_EQUAL(res, 0);
		CU_ASSERT_EQUAL(lld, TEST_VAL_I64);
		CU_ASSERT_EQUAL(llu, TEST_VAL_U64);
		free(dout.str);

		/* Invalid read (wrong type) */
		res = pomp_decoder_init(dec, &msg);
		CU_ASSERT_EQUAL(res, 0);
		res = pomp_decoder_read(dec, "%lld", &lld);
		CU_ASSERT_EQUAL(res, -EINVAL);
		res = pomp_decoder_read(dec, "%llu", &llu);
		CU_ASSERT_EQUAL(res, -EINVAL);
	}
#else
	{
		/* with wordsize == 32, %l is 32 bits */
		long ld = 0;
		unsigned long lu = 0;
		memset(&dout, 0, sizeof(dout));
		res = pomp_decoder_read(dec,
				"%hhd%hhu%hd%hu%ld%lu%"SCNd64"%"SCNu64"%ms%p%u%f%lf",
				&dout.i8, &dout.u8,
				&dout.i16, &dout.u16,
				&ld, &lu,
				&dout.i64, &dout.u64,
				&dout.str,
				&dout.cbuf, &dout.buflen,
				&dout.f32, &dout.f64);
		CU_ASSERT_EQUAL(res, 0);
		CU_ASSERT_EQUAL(ld, TEST_VAL_I32);
		CU_ASSERT_EQUAL(lu, TEST_VAL_U32);
		free(dout.str);

		/* Invalid read (wrong type) */
		res = pomp_decoder_init(dec, &msg);
		CU_ASSERT_EQUAL(res, 0);
		res = pomp_decoder_read(dec, "%ld", &ld);
		CU_ASSERT_EQUAL(res, -EINVAL);
		res = pomp_decoder_read(dec, "%lu", &lu);
		CU_ASSERT_EQUAL(res, -EINVAL);
	}
#endif

	/* Destroy */
	res = pomp_decoder_destroy(dec);
	CU_ASSERT_EQUAL(res, 0);

	/* Clear message allocated on stack */
	res = pomp_msg_clear(&msg);
	CU_ASSERT_EQUAL(res, 0);
}


/** */
static int test_decoder_walk_cb(struct pomp_decoder *dec, uint8_t type,
		const union pomp_value *v, uint32_t buflen, void *userdata)
{
	return userdata ? 1 : 0;
}

/** */
static void test_decoder_dump(void)
{
	int res = 0;
	struct pomp_msg msg = POMP_MSG_INITIALIZER;
	struct pomp_decoder *dec = NULL;
	size_t pos = 0;
	char buf[512] = "";

	/* Setup message allocated on stack */
	res = pomp_msg_init(&msg, TEST_MSGID);
	CU_ASSERT_EQUAL_FATAL(res, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(msg.buf);

	/* Allocate and init decoder */
	dec = pomp_decoder_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(dec);
	res = pomp_decoder_init(dec, &msg);
	CU_ASSERT_EQUAL(res, 0);

	/* Setup buffer */
	pos = 12;
	res = pomp_buffer_write(msg.buf, &pos, s_refdata_enc, REFDATA_ENC_SIZE);
	CU_ASSERT_EQUAL(res, 0);

	/* Dump */
	memset(buf, 0, sizeof(buf));
	res = pomp_decoder_init(dec, &msg);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_dump(dec, buf, sizeof(buf));
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_STRING_EQUAL(buf, s_msg_dump);

	/* Dump empty buffer */
	memset(buf, 0, sizeof(buf));
	res = pomp_decoder_init(dec, &msg);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_dump(dec, buf, 0);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_TRUE(buf[0] == '\0');

	/* Dump very short buffer */
	memset(buf, 0, sizeof(buf));
	res = pomp_decoder_init(dec, &msg);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_dump(dec, buf, 2);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_TRUE(strncmp(buf, s_msg_dump, 1) == 0);

	/* Dump short buffer */
	memset(buf, 0, sizeof(buf));
	res = pomp_decoder_init(dec, &msg);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_dump(dec, buf, 32);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_TRUE(strncmp(buf, s_msg_dump, 27) == 0);
	CU_ASSERT_TRUE(strncmp(buf + 27, "...}", 4) == 0);

	/* Walk complete */
	res = pomp_decoder_init(dec, &msg);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_walk(dec, &test_decoder_walk_cb, dec, 0);
	CU_ASSERT_EQUAL(res, 0);

	/* Walk partial */
	res = pomp_decoder_init(dec, &msg);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_walk(dec, &test_decoder_walk_cb, NULL, 0);
	CU_ASSERT_EQUAL(res, 0);

	/* Add an unknown argument type */
	pos = 12 + REFDATA_ENC_SIZE;
	res = pomp_buffer_writeb(msg.buf, &pos, 0x21);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid dump (unknown argument type) */
	memset(buf, 0, sizeof(buf));
	res = pomp_decoder_init(dec, &msg);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_dump(dec, buf, sizeof(buf));
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid dump/walk (NULL param) */
	res = pomp_decoder_dump(NULL, buf, sizeof(buf));
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_dump(dec, NULL, sizeof(buf));
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_walk(NULL, &test_decoder_walk_cb, NULL, 0);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_walk(dec, NULL, NULL, 0);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid dump/walk (cleared message) */
	res = pomp_msg_clear(&msg);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_dump(dec, buf, sizeof(buf));
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_walk(dec, &test_decoder_walk_cb, NULL, 0);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid dump/walk (cleared decoder) */
	res = pomp_decoder_clear(dec);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_dump(dec, buf, sizeof(buf));
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_walk(dec, &test_decoder_walk_cb, NULL, 0);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Destroy */
	res = pomp_decoder_destroy(dec);
	CU_ASSERT_EQUAL(res, 0);

	/* Clear message allocated on stack */
	res = pomp_msg_clear(&msg);
	CU_ASSERT_EQUAL(res, 0);
}

/** */
static void test_decoder_fd(void)
{
#ifndef _WIN32
	int res = 0;
	struct pomp_msg msg = POMP_MSG_INITIALIZER;
	struct pomp_decoder *dec = NULL;
	size_t pos = 0;
	int fd = -1, fdout = -1;
	uint8_t u8 = TEST_VAL_U8, u8out = 0;
	unsigned long ulout = 0;
	char buf[512] = "";

	/* Setup message allocated on stack */
	res = pomp_msg_init(&msg, TEST_MSGID);
	CU_ASSERT_EQUAL_FATAL(res, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(msg.buf);

	/* Allocate and init decoder */
	dec = pomp_decoder_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(dec);
	res = pomp_decoder_init(dec, &msg);
	CU_ASSERT_EQUAL(res, 0);

	/* Setup buffer, add a u8 and a fd (stderr) */
	pos = 12;
	res = pomp_buffer_writeb(msg.buf, &pos, 0x02);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_buffer_writeb(msg.buf, &pos, u8);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_buffer_writeb(msg.buf, &pos, 0x0d);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_buffer_write_fd(msg.buf, &pos, 2);
	CU_ASSERT_EQUAL(res, 0);
	fd = pomp_buffer_get_fd(msg.buf, pos - 4);

	/* Read */
	res = pomp_decoder_read_u8(dec, &u8out);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_read_fd(dec, &fdout);
	CU_ASSERT_EQUAL(res, 0);

	/* Check */
	CU_ASSERT_EQUAL(fdout, fd);

	/* Read */
	res = pomp_decoder_init(dec, &msg);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_read(dec, "%hhu%x", &u8out, &fdout);
	CU_ASSERT_EQUAL(res, 0);

	/* Check */
	CU_ASSERT_EQUAL(fdout, fd);

	/* Walk with fd checks */
	res = pomp_decoder_init(dec, &msg);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_walk(dec, &test_decoder_walk_cb, dec, 1);
	CU_ASSERT_EQUAL(res, 0);

	/* Dump */
	memset(buf, 0, sizeof(buf));
	res = pomp_decoder_init(dec, &msg);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_dump(dec, buf, sizeof(buf));
	CU_ASSERT_EQUAL(res, 0);

	/* Walk without fd checks */
	res = pomp_decoder_init(dec, &msg);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_walk(dec, &test_decoder_walk_cb, dec, 0);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid read (end of buffer) */
	res = pomp_decoder_read_fd(dec, &fdout);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid read (NULL param) */
	res = pomp_decoder_read_fd(NULL, &fdout);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_decoder_read_fd(dec, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid read (wrong type) */
	res = pomp_decoder_init(dec, &msg);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_read_fd(dec, &fdout);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid read (invalid format width) */
	res = pomp_decoder_read(dec, "%lx", &ulout);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid read (wrong type) */
	res = pomp_decoder_init(dec, &msg);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_decoder_read(dec, "%x", &fdout);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Clear */
	res = pomp_decoder_clear(dec);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid read (cleared decoder) */
	res = pomp_decoder_read_fd(dec, &fdout);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Destroy */
	res = pomp_decoder_destroy(dec);
	CU_ASSERT_EQUAL(res, 0);

	/* Clear message allocated on stack */
	res = pomp_msg_clear(&msg);
	CU_ASSERT_EQUAL(res, 0);
#endif /* !_WIN32 */
}

/** */
static void verify_test_msg(const struct pomp_msg *msg)
{
	int res = 0;
	uint32_t msgid = 0;
	struct test_data dout;

	msgid = pomp_msg_get_id(msg);
	CU_ASSERT_EQUAL(msgid, TEST_MSGID);

	/* Read */
	memset(&dout, 0, sizeof(dout));
	res = pomp_msg_read(msg,
		"%hhd%hhu%hd%hu%d%u%"SCNd64"%"SCNu64"%ms%p%u%f%lf",
		&dout.i8, &dout.u8,
		&dout.i16, &dout.u16,
		&dout.i32, &dout.u32,
		&dout.i64, &dout.u64,
		&dout.str,
		&dout.cbuf, &dout.buflen,
		&dout.f32, &dout.f64);
	CU_ASSERT_EQUAL_FATAL(res, 0);

	/* Check */
	CU_ASSERT_EQUAL(dout.i8, TEST_VAL_I8);
	CU_ASSERT_EQUAL(dout.u8, TEST_VAL_U8);
	CU_ASSERT_EQUAL(dout.i16, TEST_VAL_I16);
	CU_ASSERT_EQUAL(dout.u16, TEST_VAL_U16);
	CU_ASSERT_EQUAL(dout.i32, TEST_VAL_I32);
	CU_ASSERT_EQUAL(dout.u32, TEST_VAL_U32);
	CU_ASSERT_EQUAL(dout.i64, TEST_VAL_I64);
	CU_ASSERT_EQUAL(dout.u64, TEST_VAL_U64);
	CU_ASSERT_STRING_EQUAL(dout.str, TEST_VAL_STR);
	CU_ASSERT_EQUAL(dout.buflen, TEST_VAL_BUFLEN);
	CU_ASSERT_EQUAL(memcmp(dout.cbuf, TEST_VAL_BUF, TEST_VAL_BUFLEN), 0);
	CU_ASSERT_EQUAL(dout.f32, TEST_VAL_F32);
	CU_ASSERT_EQUAL(dout.f64, TEST_VAL_F64);
	free(dout.str);
}

/** */
static void verify_test_msg_no_payload(const struct pomp_msg *msg)
{
	uint32_t msgid = 0;

	msgid = pomp_msg_get_id(msg);
	CU_ASSERT_EQUAL(msgid, TEST_MSGID);
}

/** */
static void setup_test_buf(struct pomp_buffer *buf)
{
	int res = 0;
	size_t pos = 0;

	/* First message */
	res = pomp_buffer_write(buf, &pos, s_refdata_enc_header, 12);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_buffer_write(buf, &pos, s_refdata_enc, REFDATA_ENC_SIZE);
	CU_ASSERT_EQUAL(res, 0);

	/* Second message */
	res = pomp_buffer_write(buf, &pos, s_refdata_enc_header, 12);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_buffer_write(buf, &pos, s_refdata_enc, REFDATA_ENC_SIZE);
	CU_ASSERT_EQUAL(res, 0);
}

/** */
static void setup_test_buf_no_payload(struct pomp_buffer *buf)
{
	int res = 0;
	size_t pos = 0;

	/* First message */
	res = pomp_buffer_write(buf, &pos, s_refdata_enc_header_no_payload, 12);
	CU_ASSERT_EQUAL(res, 0);

	/* Second message */
	res = pomp_buffer_write(buf, &pos, s_refdata_enc_header_no_payload, 12);
	CU_ASSERT_EQUAL(res, 0);
}

/** */
static void test_prot_base(void)
{
	int res = 0;
	struct pomp_buffer *buf = NULL;
	ssize_t declen = 0;
	struct pomp_prot *prot = NULL;
	struct pomp_msg *msg = NULL;

	/* Creation */
	prot = pomp_prot_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(prot);

	/* Setup buffer */
	buf = pomp_buffer_new(0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
	setup_test_buf(buf);

	/* Decode */
	declen = pomp_prot_decode_msg(prot, buf->data,
			12 + REFDATA_ENC_SIZE, &msg);
	CU_ASSERT_EQUAL(declen, 12 + REFDATA_ENC_SIZE);
	CU_ASSERT_PTR_NOT_NULL(msg);

	/* Check message */
	verify_test_msg(msg);

	/* Invalid decode (NULL param) */
	declen = pomp_prot_decode_msg(NULL, buf, 12 + REFDATA_ENC_SIZE, &msg);
	CU_ASSERT_EQUAL(declen, -EINVAL);
	declen = pomp_prot_decode_msg(prot, NULL, 0, &msg);
	CU_ASSERT_EQUAL(declen, -EINVAL);
	declen = pomp_prot_decode_msg(prot, buf, 12 + REFDATA_ENC_SIZE, NULL);
	CU_ASSERT_EQUAL(declen, -EINVAL);

	/* Release message */
	res = pomp_prot_release_msg(prot, msg);
	CU_ASSERT_EQUAL(res, 0);

	/* Release another message (direct destroy without ownership in prot) */
	msg = pomp_msg_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(msg);
	res = pomp_prot_release_msg(prot, msg);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid release (NULL param) */
	res = pomp_prot_release_msg(NULL, msg);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_prot_release_msg(prot, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Destroy */
	res = pomp_prot_destroy(prot);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid destroy (NULL param) */
	res = pomp_prot_destroy(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Creation and destroy with no decoding phase */
	prot = pomp_prot_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(prot);
	res = pomp_prot_destroy(prot);
	CU_ASSERT_EQUAL(res, 0);

	pomp_buffer_unref(buf);
}

/** */
static void test_prot_decode(void)
{
	struct pomp_buffer *buf = NULL;
	size_t pos = 0;
	int res = 0;
	ssize_t declen = 0;
	struct pomp_prot *prot = NULL;
	struct pomp_msg *msg = NULL;

	/* Creation */
	prot = pomp_prot_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(prot);

	/* Setup buffer */
	buf = pomp_buffer_new(0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
	setup_test_buf(buf);

	/* Decode full buffer */
	pos = 0;
	while (pos < buf->len) {
		msg = NULL;
		declen = pomp_prot_decode_msg(prot, buf->data + pos,
				buf->len - pos, &msg);
		CU_ASSERT_EQUAL(declen, 12 + REFDATA_ENC_SIZE);
		pos += (size_t)declen;
		CU_ASSERT_PTR_NOT_NULL(msg);
		verify_test_msg(msg);
		res = pomp_msg_destroy(msg);
		CU_ASSERT_EQUAL(res, 0);
	}

	/* Decode 1 byte at a time */
	pos = 0;
	while (pos < buf->len) {
		msg = NULL;
		declen = pomp_prot_decode_msg(prot, buf->data + pos, 1, &msg);
		CU_ASSERT_EQUAL(declen, 1);
		pos += 1;
		if (pos % (12 + REFDATA_ENC_SIZE) == 0) {
			CU_ASSERT_PTR_NOT_NULL(msg);
			verify_test_msg(msg);
			res = pomp_msg_destroy(msg);
			CU_ASSERT_EQUAL(res, 0);
		} else {
			CU_ASSERT_PTR_NULL(msg);
		}
	}

	/* Free */
	res = pomp_prot_destroy(prot);
	CU_ASSERT_EQUAL(res, 0);
	pomp_buffer_unref(buf);
}

/** */
static void test_prot_decode_no_payload(void)
{
	struct pomp_buffer *buf = NULL;
	size_t pos = 0;
	int res = 0;
	ssize_t declen = 0;
	struct pomp_prot *prot = NULL;
	struct pomp_msg *msg = NULL;

	/* Creation */
	prot = pomp_prot_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(prot);

	/* Setup buffer */
	buf = pomp_buffer_new(0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
	setup_test_buf_no_payload(buf);

	/* Decode full buffer */
	pos = 0;
	while (pos < buf->len) {
		msg = NULL;
		declen = pomp_prot_decode_msg(prot, buf->data + pos,
				buf->len - pos, &msg);
		CU_ASSERT_EQUAL(declen, 12);
		pos += (size_t)declen;
		CU_ASSERT_PTR_NOT_NULL(msg);
		verify_test_msg_no_payload(msg);
		res = pomp_msg_destroy(msg);
		CU_ASSERT_EQUAL(res, 0);
	}

	/* Decode 1 byte at a time */
	pos = 0;
	while (pos < buf->len) {
		msg = NULL;
		declen = pomp_prot_decode_msg(prot, buf->data + pos, 1, &msg);
		CU_ASSERT_EQUAL(declen, 1);
		pos += 1;
		if (pos % 12 == 0) {
			CU_ASSERT_PTR_NOT_NULL(msg);
			verify_test_msg_no_payload(msg);
			res = pomp_msg_destroy(msg);
			CU_ASSERT_EQUAL(res, 0);
		} else {
			CU_ASSERT_PTR_NULL(msg);
		}
	}

	/* Free */
	res = pomp_prot_destroy(prot);
	CU_ASSERT_EQUAL(res, 0);
	pomp_buffer_unref(buf);
}

/** */
static void test_prot_decode_error(void)
{
	struct pomp_buffer *buf = NULL;
	size_t pos = 0, i = 0;
	int res = 0;
	ssize_t declen = 0;
	struct pomp_prot *prot = NULL;
	struct pomp_msg *msg = NULL;

	/* Creation */
	prot = pomp_prot_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(prot);

	/* Create buffer */
	buf = pomp_buffer_new(0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(buf);

	/* Corrupt each magic bytes */
	for (i = 0; i < 4; i++) {
		/* Setup buffer */
		setup_test_buf(buf);
		pos = i;
		res = pomp_buffer_writeb(buf, &pos, 0);
		CU_ASSERT_EQUAL(res, 0);

		/* Decode */
		msg = NULL;
		declen = pomp_prot_decode_msg(prot, buf->data,
				12 + REFDATA_ENC_SIZE, &msg);
		CU_ASSERT_EQUAL(declen, 12 + REFDATA_ENC_SIZE);
		CU_ASSERT_PTR_NULL(msg);
	}

	/* Corrupt size */
	{
		/* Setup buffer */
		setup_test_buf(buf);
		pos = 8;
		pomp_buffer_writeb(buf, &pos, 0);
		CU_ASSERT_EQUAL(res, 0);

		/* Decode */
		msg = NULL;
		declen = pomp_prot_decode_msg(prot, buf->data,
				12 + REFDATA_ENC_SIZE, &msg);
		CU_ASSERT_EQUAL(declen, 12 + REFDATA_ENC_SIZE);
		CU_ASSERT_PTR_NULL(msg);
	}

	/* Free */
	res = pomp_prot_destroy(prot);
	CU_ASSERT_EQUAL(res, 0);
	pomp_buffer_unref(buf);
}

/* Disable some gcc warnings for test suite descriptions */
#ifdef __GNUC__
#  pragma GCC diagnostic ignored "-Wcast-qual"
#endif /* __GNUC__ */

/** */
static CU_TestInfo s_buffer_tests[] = {
	{(char *)"base", &test_buffer_base},
	{(char *)"read_write", &test_buffer_read_write},
	{(char *)"perm", &test_buffer_perm},
	{(char *)"fd", &test_buffer_fd},
	CU_TEST_INFO_NULL,
};

/** */
static CU_TestInfo s_msg_tests[] = {
	{(char *)"base", &test_msg_base},
	{(char *)"advanced", &test_msg_advanced},
	{(char *)"read_write", &test_msg_read_write},
	{(char *)"read_write_no_payload", &test_msg_read_write_no_payload},
	{(char *)"write_argv", &test_msg_write_argv},
	CU_TEST_INFO_NULL,
};

/** */
static CU_TestInfo s_encoder_tests[] = {
	{(char *)"base", &test_encoder_base},
	{(char *)"printf", &test_encoder_printf},
	{(char *)"printf_no_payload", &test_encoder_printf_no_payload},
	{(char *)"printf_32_64", &test_encoder_printf_32_64},
	{(char *)"argv", &test_encoder_argv},
	{(char *)"fd", &test_encoder_fd},
	CU_TEST_INFO_NULL,
};

/** */
static CU_TestInfo s_decoder_tests[] = {
	{(char *)"base", &test_decoder_base},
	{(char *)"partial", &test_decoder_partial},
	{(char *)"scanf", &test_decoder_scanf},
	{(char *)"scanf_no_payload", &test_decoder_scanf_no_payload},
	{(char *)"scanf_32_64", &test_decoder_scanf_32_64},
	{(char *)"dump", &test_decoder_dump},
	{(char *)"fd", &test_decoder_fd},
	CU_TEST_INFO_NULL,
};

/** */
static CU_TestInfo s_prot_tests[] = {
	{(char *)"base", &test_prot_base},
	{(char *)"decode", &test_prot_decode},
	{(char *)"decode_no_payload", &test_prot_decode_no_payload},
	{(char *)"decode_error", &test_prot_decode_error},
	CU_TEST_INFO_NULL,
};

/** */
/*extern*/ CU_SuiteInfo g_suites_basic[] = {
	{(char *)"buffer", NULL, NULL, s_buffer_tests},
	{(char *)"msg", NULL, NULL, s_msg_tests},
	{(char *)"encoder", NULL, NULL, s_encoder_tests},
	{(char *)"decoder", NULL, NULL, s_decoder_tests},
	{(char *)"prot", NULL, NULL, s_prot_tests},
	CU_SUITE_INFO_NULL,
};
