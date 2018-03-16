/**
 *  Copyright (c) 2018 Parrot Drones SAS
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
 */

#include "pomp_test.h"

/** */
struct test_data {
	struct pomp_evt *evt;
	uint64_t counter;
};

/** */
static void test_event_cb(int fd, uint32_t revents, void *userdata)
{
	struct test_data *data = userdata;

	int res = pomp_evt_clear(data->evt);
	CU_ASSERT_EQUAL(res, 0);

	data->counter++;
}

/** */
static void test_event(void)
{
	int res = 0;
	int fd;
	struct test_data data;
	struct pomp_loop *loop = NULL;
	struct pomp_evt *evt = NULL;

	memset(&data, 0, sizeof(data));

	/* Create loop */
	loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(loop);

	/* Create event */
	evt = pomp_evt_new();
	CU_ASSERT_PTR_NOT_NULL(evt);

	/* Save event into data */
	data.evt = evt;

	/* Get event fd */
	fd = pomp_evt_get_fd(evt);

	/* Add fd to loop */
	res = pomp_loop_add(loop, fd, POMP_FD_EVENT_IN, test_event_cb, &data);
	CU_ASSERT_EQUAL(res, 0);

	/* Shall not fire before signal */
	res = pomp_loop_wait_and_process(loop, 250);
	CU_ASSERT_EQUAL(res, -ETIMEDOUT);
	CU_ASSERT_EQUAL(data.counter, 0);

	/* Signal the event */
	res = pomp_evt_signal(evt);
	CU_ASSERT_EQUAL(res, 0);

	/* Shall fire once */
	res = pomp_loop_wait_and_process(loop, 250);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(data.counter, 1);

	/* Signal the twice */
	res = pomp_evt_signal(evt);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_evt_signal(evt);
	CU_ASSERT_EQUAL(res, 0);

	/* Shall fire only once */
	res = pomp_loop_wait_and_process(loop, 250);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(data.counter, 2);

	/* Shall not fire */
	res = pomp_loop_wait_and_process(loop, 250);
	CU_ASSERT_EQUAL(res, -ETIMEDOUT);
	CU_ASSERT_EQUAL(data.counter, 2);

	/* Signal the event */
	res = pomp_evt_signal(evt);
	CU_ASSERT_EQUAL(res, 0);

	/* Clear the event */
	res = pomp_evt_clear(evt);
	CU_ASSERT_EQUAL(res, 0);

	/* Shall not fire */
	res = pomp_loop_wait_and_process(loop, 250);
	CU_ASSERT_EQUAL(res, -ETIMEDOUT);
	CU_ASSERT_EQUAL(data.counter, 2);

	/* Clear on an unsignalled event */
	res = pomp_evt_clear(evt);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid get_fd (NULL param) */
	res = pomp_evt_get_fd(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid clear (NULL param) */
	res = pomp_evt_clear(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid signal (NULL param) */
	res = pomp_evt_signal(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Remove fd from loop */
	res = pomp_loop_remove(loop, fd);
	CU_ASSERT_EQUAL(res, 0);

	/* Destroy event */
	res = pomp_evt_destroy(evt);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid destroy (NULL param) */
	res = pomp_evt_destroy(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Destroy loop */
	res = pomp_loop_destroy(loop);
	CU_ASSERT_EQUAL(res, 0);
}

/** */
#ifdef POMP_HAVE_EVENT_FD
static void test_evt_eventfd(void)
{
	const struct pomp_evt_ops *evt_ops = NULL;
	evt_ops = pomp_evt_set_ops(&pomp_evt_fd_ops);
	test_event();
	pomp_evt_set_ops(evt_ops);
}
#endif /* POMP_HAVE_EVENT_FD */

/** */
#ifdef POMP_HAVE_EVENT_POSIX
static void test_evt_posix(void)
{
	const struct pomp_evt_ops *evt_ops = NULL;
	evt_ops = pomp_evt_set_ops(&pomp_evt_posix_ops);
	test_event();
	pomp_evt_set_ops(evt_ops);
}
#endif /* POMP_HAVE_EVENT_POSIX */

/** */
#ifdef POMP_HAVE_EVENT_WIN32
static void test_evt_win32(void)
{
	const struct pomp_evt_ops *evt_ops = NULL;
	evt_ops = pomp_evt_set_ops(&pomp_evt_win32_ops);
	test_event();
	pomp_evt_set_ops(evt_ops);
}
#endif /* POMP_HAVE_EVENT_WIN32 */

/* Disable some gcc warnings for test suite descriptions */
#ifdef __GNUC__
#  pragma GCC diagnostic ignored "-Wcast-qual"
#endif /* __GNUC__ */

/** */
static CU_TestInfo s_evt_tests[] = {

#ifdef POMP_HAVE_EVENT_FD
	{(char *)"eventfd", &test_evt_eventfd},
#endif /* POMP_HAVE_EVENT_FD */

#ifdef POMP_HAVE_EVENT_POSIX
	{(char *)"posix", &test_evt_posix},
#endif /* POMP_HAVE_EVENT_POSIX */

#ifdef POMP_HAVE_EVENT_WIN32
	{(char *)"win32", &test_evt_win32},
#endif /* POMP_HAVE_EVENT_WIN32 */

	CU_TEST_INFO_NULL,
};

/** */
/*extern*/ CU_SuiteInfo g_suites_evt[] = {
	{(char *)"event", NULL, NULL, s_evt_tests},
	CU_SUITE_INFO_NULL,
};
