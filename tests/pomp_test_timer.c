/**
 * @file pomp_test_timer.c
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
	uint32_t  counter;
};

/** */
static void timer_cb(struct pomp_timer *timer, void *userdata)
{
	struct test_data *data = userdata;
	data->counter++;
}

/** */
static void test_timer(void)
{
	int res = 0;
	struct test_data data;
	struct pomp_loop *loop = NULL;
	struct pomp_timer *timer = NULL;
	struct pomp_timer *timer2 = NULL;

	memset(&data, 0, sizeof(data));

	/* Create loop */
	loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(loop);

	/* Create timer */
	timer = pomp_timer_new(loop, &timer_cb, &data);
	CU_ASSERT_PTR_NOT_NULL(timer);

	/* Invalid create (NULL param) */
	timer2 = pomp_timer_new(NULL, &timer_cb, &data);
	CU_ASSERT_PTR_NULL(timer2);
	timer2 = pomp_timer_new(loop, NULL, &data);
	CU_ASSERT_PTR_NULL(timer2);

	/* Setup timer */
	res = pomp_timer_set(timer, 500);
	CU_ASSERT_EQUAL(res, 0);

	/* Shall not fire before 250ms */
	res = pomp_loop_wait_and_process(loop, 250);
	CU_ASSERT_EQUAL(res, -ETIMEDOUT);
	CU_ASSERT_EQUAL(data.counter, 0);

	/* Shall fire before 500ms (750 from setup) */
	res = pomp_loop_wait_and_process(loop, 500);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(data.counter, 1);

	/* Setup timer */
	res = pomp_timer_set(timer, 500);
	CU_ASSERT_EQUAL(res, 0);

	/* Shall not fire before 250ms */
	res = pomp_loop_wait_and_process(loop, 250);
	CU_ASSERT_EQUAL(res, -ETIMEDOUT);
	CU_ASSERT_EQUAL(data.counter, 1);

	/* Cancel it */
	res = pomp_timer_clear(timer);
	CU_ASSERT_EQUAL(res, 0);

	/* Shall not fire after 500ms (750 from setup) */
	res = pomp_loop_wait_and_process(loop, 500);
	CU_ASSERT_EQUAL(res, -ETIMEDOUT);
	CU_ASSERT_EQUAL(data.counter, 1);

	/* Periodic timer */
	data.counter = 0;
	res = pomp_timer_set_periodic(timer, 500, 1000);
	CU_ASSERT_EQUAL(res, 0);

	/* First trigger */
	res = pomp_loop_wait_and_process(loop, 750);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(data.counter, 1);

	/* Second trigger shall not fire before 1000ms */
	res = pomp_loop_wait_and_process(loop, 750);
	CU_ASSERT_EQUAL(res, -ETIMEDOUT);
	CU_ASSERT_EQUAL(data.counter, 1);

	/* Second trigger */
	res = pomp_loop_wait_and_process(loop, 500);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(data.counter, 2);

	/* Third trigger */
	res = pomp_loop_wait_and_process(loop, 1250);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(data.counter, 3);

	/* Cancel it */
	res = pomp_timer_clear(timer);
	CU_ASSERT_EQUAL(res, 0);

	/* Shall not fire anymore */
	res = pomp_loop_wait_and_process(loop, 1250);
	CU_ASSERT_EQUAL(res, -ETIMEDOUT);
	CU_ASSERT_EQUAL(data.counter, 3);

	/* Invalid set (NULL param) */
	res = pomp_timer_set(NULL, 500);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid clear (NULL param) */
	res = pomp_timer_clear(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Destroy timer */
	res = pomp_timer_destroy(timer);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid destroy (NULL param) */
	res = pomp_timer_destroy(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Destroy loop */
	res = pomp_loop_destroy(loop);
	CU_ASSERT_EQUAL(res, 0);
}

/** */
#ifdef POMP_HAVE_TIMER_FD
static void test_timer_timerfd(void)
{
	const struct pomp_timer_ops *timer_ops = NULL;
	timer_ops = pomp_timer_set_ops(&pomp_timer_fd_ops);
	test_timer();
	pomp_timer_set_ops(timer_ops);
}
#endif /* POMP_HAVE_TIMER_FD */

/** */
#ifdef POMP_HAVE_TIMER_KQUEUE
static void test_timer_kqueue(void)
{
	const struct pomp_timer_ops *timer_ops = NULL;
	timer_ops = pomp_timer_set_ops(&pomp_timer_kqueue_ops);
	test_timer();
	pomp_timer_set_ops(timer_ops);
}
#endif /* POMP_HAVE_TIMER_KQUEUE */

/** */
#ifdef POMP_HAVE_TIMER_POSIX
static void test_timer_posix(void)
{
	const struct pomp_timer_ops *timer_ops = NULL;
	timer_ops = pomp_timer_set_ops(&pomp_timer_posix_ops);
	test_timer();
	pomp_timer_set_ops(timer_ops);
}
#endif /* POMP_HAVE_TIMER_POSIX */

/** */
#ifdef POMP_HAVE_TIMER_WIN32
static void test_timer_win32(void)
{
	const struct pomp_timer_ops *timer_ops = NULL;
	timer_ops = pomp_timer_set_ops(&pomp_timer_win32_ops);
	test_timer();
	pomp_timer_set_ops(timer_ops);
}
#endif /* POMP_HAVE_TIMER_WIN32 */

/* Disable some gcc warnings for test suite descriptions */
#ifdef __GNUC__
#  pragma GCC diagnostic ignored "-Wcast-qual"
#endif /* __GNUC__ */

/** */
static CU_TestInfo s_timer_tests[] = {

#ifdef POMP_HAVE_TIMER_FD
	{(char *)"timerfd", &test_timer_timerfd},
#endif /* POMP_HAVE_TIMER_FD */

#ifdef POMP_HAVE_TIMER_KQUEUE
	{(char *)"kqueue", &test_timer_kqueue},
#endif /* POMP_HAVE_TIMER_KQUEUE */

#ifdef POMP_HAVE_TIMER_POSIX
	{(char *)"posix", &test_timer_posix},
#endif /* POMP_HAVE_TIMER_POSIX */

#ifdef POMP_HAVE_TIMER_WIN32
	{(char *)"win32", &test_timer_win32},
#endif /* POMP_HAVE_TIMER_WIN32 */

	CU_TEST_INFO_NULL,
};

/** */
/*extern*/ CU_SuiteInfo g_suites_timer[] = {
	{(char *)"timer", NULL, NULL, s_timer_tests},
	CU_SUITE_INFO_NULL,
};
