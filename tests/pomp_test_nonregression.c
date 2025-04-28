/**
 * @file pomp_test_nonregression.c
 *
 * @author hugo.grostabussiat@parrot.com
 *
 * Copyright (c) 2021 Parrot S.A.
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


/* Disable some gcc warnings for test suite descriptions */
#ifdef __GNUC__
#  pragma GCC diagnostic ignored "-Wcast-qual"
#endif /* __GNUC__ */

static void stop_in_callback_cli_cbs(struct pomp_ctx *pctx,
		enum pomp_event event, struct pomp_conn *conn,
		const struct pomp_msg *msg, void *userdata)
{
	switch (event) {
	case POMP_EVENT_CONNECTED:
		break;
	case POMP_EVENT_DISCONNECTED:
		break;
	case POMP_EVENT_MSG:
		pomp_ctx_stop(pctx);
		break;
	default:
		break;
	}
}

static void stop_in_callback_srv_cbs(struct pomp_ctx *pctx,
		enum pomp_event event, struct pomp_conn *conn,
		const struct pomp_msg *msg, void *userdata)
{
	int res;
	int *stop = userdata;
	switch (event) {
	case POMP_EVENT_CONNECTED:
		res = pomp_ctx_send(pctx, 1638, "%u", 0x76543210);
		CU_ASSERT_EQUAL(res, 0);
		break;
	case POMP_EVENT_DISCONNECTED:
		*stop = 1;
		break;
	case POMP_EVENT_MSG:
		break;
	default:
		break;
	}
}

/** Don't crash on pomp_ctx_destroy() from within MSG event callback */
static void test_ctx_stop_in_callback(void)
{
	struct pomp_loop *loop;
	struct pomp_ctx *srv_ctx;
	struct pomp_ctx *cli_ctx;
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_port = htons(1234),
		.sin_addr = {
			.s_addr = htonl(INADDR_LOOPBACK),
		},
	};
	int stop = 0;
	int res;

	loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(loop);
	srv_ctx = pomp_ctx_new_with_loop(stop_in_callback_srv_cbs, &stop, loop);
	CU_ASSERT_PTR_NOT_NULL_FATAL(srv_ctx);
	cli_ctx = pomp_ctx_new_with_loop(stop_in_callback_cli_cbs, NULL, loop);
	CU_ASSERT_PTR_NOT_NULL_FATAL(cli_ctx);

	res = pomp_ctx_listen(srv_ctx, (struct sockaddr *)&sin, sizeof(sin));
	CU_ASSERT_EQUAL_FATAL(res, 0);
	res = pomp_ctx_connect(cli_ctx, (struct sockaddr *)&sin, sizeof(sin));
	CU_ASSERT_EQUAL_FATAL(res, 0);

	while (!stop) {
		res = pomp_loop_wait_and_process(loop, 5000);
		CU_ASSERT_EQUAL_FATAL(res, 0);
	}

	res = pomp_ctx_stop(srv_ctx);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_ctx_stop(cli_ctx);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_ctx_destroy(srv_ctx);
	CU_ASSERT_EQUAL(res, 0);
	srv_ctx = NULL;
	res = pomp_ctx_destroy(cli_ctx);
	CU_ASSERT_EQUAL(res, 0);
	cli_ctx = NULL;
	pomp_loop_destroy(loop);
	loop = NULL;
}

/** */
static CU_TestInfo s_ctx_tests[] = {
	{(char *)"ctx_stop_in_callback", &test_ctx_stop_in_callback},
	CU_TEST_INFO_NULL,
};

/** */
/*extern*/ CU_SuiteInfo g_suites_nonregression[] = {
	{(char *)"nonregression", NULL, NULL, s_ctx_tests},
	CU_SUITE_INFO_NULL,
};

