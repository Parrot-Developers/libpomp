/**
 * @file pomp_test_addr.c
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

/** */
struct test_data {
	const char		*str;
	const struct sockaddr	*addr;
	uint32_t		addrlen;
};

/** */
static const char s_inet_saddr_str[] = "inet:10.201.4.100:1234";
static const struct sockaddr_in s_inet_saddr = {
	.sin_family = AF_INET,
	.sin_port = 0xd204,
	.sin_addr.s_addr = 0x6404c90a,
};

static const char s_inet6_saddr_str[] = "inet6:fe80::5842:5cff:fe6b:ec7e:1234";
static const struct sockaddr_in6 s_inet6_saddr = {
	.sin6_family = AF_INET6,
	.sin6_port = 0xd204,
	.sin6_addr.s6_addr = {0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x58, 0x42, 0x5c, 0xff, 0xfe, 0x6b, 0xec, 0x7e},
};

static const char s_unix_saddr_str_1[] = "unix:/tmp/foo";
static const struct sockaddr_un s_unix_saddr_1 = {
	.sun_family = AF_UNIX,
	.sun_path = "/tmp/foo",
};

static const char s_unix_saddr_str_2[] = "unix:@/tmp/foo";
static const struct sockaddr_un s_unix_saddr_2 = {
	.sun_family = AF_UNIX,
	.sun_path = "\0/tmp/foo",
};

/** */
static const struct test_data s_data[] = {
	{s_inet_saddr_str,
		(const struct sockaddr *)&s_inet_saddr,
		sizeof(s_inet_saddr)},
	{s_inet6_saddr_str,
		(const struct sockaddr *)&s_inet6_saddr,
		sizeof(s_inet6_saddr)},
	{s_unix_saddr_str_1,
		(const struct sockaddr *)&s_unix_saddr_1,
		sizeof(s_unix_saddr_1)},
	{s_unix_saddr_str_2,
		(const struct sockaddr *)&s_unix_saddr_2,
		sizeof(s_unix_saddr_2)},
};

/** */
static void test_addr(void)
{
	int res = 0;
	size_t i = 0;
	struct sockaddr_storage addr;
	uint32_t addrlen = 0;
	char buf[156] = "";

	for (i = 0; i < sizeof(s_data) / sizeof(s_data[0]); i++) {
		memset(&addr, 0, sizeof(addr));

		/* Parse, input length bigger that needed */
		addrlen = sizeof(addr);
		res = pomp_addr_parse(s_data[i].str, (struct sockaddr *)&addr, &addrlen);
		CU_ASSERT_EQUAL(res, 0);
		CU_ASSERT_EQUAL(addrlen, s_data[i].addrlen);

		/* Compare only after familiy field (FreeBSD has an extra len field) */
		CU_ASSERT_EQUAL(memcmp(&addr.ss_family,
				&s_data[i].addr->sa_family,
				s_data[i].addrlen - offsetof(struct sockaddr, sa_family)), 0);

		/* Parse, input length exactly needed */
		addrlen = s_data[i].addrlen;
		res = pomp_addr_parse(s_data[i].str, (struct sockaddr *)&addr, &addrlen);
		CU_ASSERT_EQUAL(res, 0);
		CU_ASSERT_EQUAL(addrlen, s_data[i].addrlen);

		/* Compare only after familiy field (FreeBSD has an extra len field) */
		CU_ASSERT_EQUAL(memcmp(&addr.ss_family,
				&s_data[i].addr->sa_family,
				s_data[i].addrlen - offsetof(struct sockaddr, sa_family)), 0);

		/* Parse, input length smaller that needed */
		addrlen = s_data[i].addrlen - 1;
		res = pomp_addr_parse(s_data[i].str, (struct sockaddr *)&addr, &addrlen);
		CU_ASSERT_EQUAL(res, -EINVAL);

		/* Format */
		res = pomp_addr_format(buf, sizeof(buf), s_data[i].addr, s_data[i].addrlen);
		CU_ASSERT_EQUAL(res, 0);
		CU_ASSERT_STRING_EQUAL(buf, s_data[i].str);

		/* pomp_addr_is_unix */
		CU_ASSERT_EQUAL(pomp_addr_is_unix(s_data[i].addr, s_data[i].addrlen),
				s_data[i].addr->sa_family == AF_UNIX);
	}

	/* Parse, bad string */
	addrlen = sizeof(addr);
	res = pomp_addr_parse("inet:a.b.c.d", (struct sockaddr *)&addr, &addrlen);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_addr_parse("inet:a.b.c.d:p", (struct sockaddr *)&addr, &addrlen);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_addr_parse("inet:a.b.c.d:1234", (struct sockaddr *)&addr, &addrlen);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_addr_parse("inet6:::1:p", (struct sockaddr *)&addr, &addrlen);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_addr_parse("inet6:::1", (struct sockaddr *)&addr, &addrlen);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_addr_parse("foo", (struct sockaddr *)&addr, &addrlen);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Parse, invalid parameter */
	res = pomp_addr_parse(NULL, (struct sockaddr *)&addr, &addrlen);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_addr_parse("", NULL, &addrlen);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_addr_parse("", (struct sockaddr *)&addr, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);
	addrlen = 0;
	res = pomp_addr_parse("", (struct sockaddr *)&addr, &addrlen);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Format, invalid parameter */
	res = pomp_addr_format(NULL, 0, s_data[0].addr, s_data[0].addrlen);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_addr_format(buf, 0, s_data[0].addr, s_data[0].addrlen);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_addr_format(buf, sizeof(buf), NULL, 0);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_addr_format(buf, sizeof(buf), s_data[0].addr, 0);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_addr_format(buf, sizeof(buf), (const struct sockaddr *)&s_unix_saddr_1, sizeof(struct sockaddr));
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Format, unknown family */
	memset(&addr, 0, sizeof(addr));
	addr.ss_family = 42;
	res = pomp_addr_format(buf, sizeof(buf), (const struct sockaddr *)&addr, sizeof(struct sockaddr));
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_STRING_EQUAL(buf, "addr:family:42");

	/* pomp_addr_is_unix, invalid parameter */
	res = pomp_addr_is_unix(NULL, sizeof(s_unix_saddr_1));
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_addr_is_unix((const struct sockaddr *)&s_unix_saddr_1, 0);
	CU_ASSERT_EQUAL(res, 0);
}

/* Disable some gcc warnings for test suite descriptions */
#ifdef __GNUC__
#  pragma GCC diagnostic ignored "-Wcast-qual"
#endif /* __GNUC__ */

/** */
static CU_TestInfo s_addr_tests[] = {
	{(char *)"addr", &test_addr},
	CU_TEST_INFO_NULL,
};

/** */
/*extern*/ CU_SuiteInfo g_suites_addr[] = {
	{(char *)"addr", NULL, NULL, s_addr_tests},
	CU_SUITE_INFO_NULL,
};
