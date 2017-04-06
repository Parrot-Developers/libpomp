/**
 * @file pomp_test_loop.c
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

#ifdef __linux__

/** */
static int setup_timerfd(uint32_t delay, uint32_t period)
{
	int res = 0;
	int tfd = -1;
	struct itimerspec newval, oldval;

	tfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
	CU_ASSERT_TRUE_FATAL(tfd >= 0);

	/* Setup timeout */
	newval.it_interval.tv_sec = (time_t)(period / 1000);
	newval.it_interval.tv_nsec = (long int)((period % 1000) * 1000 * 1000);
	newval.it_value.tv_sec = (time_t)(delay / 1000);
	newval.it_value.tv_nsec = (long int)((delay % 1000) * 1000 * 1000);
	res = timerfd_settime(tfd, 0, &newval, &oldval);
	CU_ASSERT_EQUAL_FATAL(res, 0);

	return tfd;
}

/** */
static void timerfd_cb(int fd, uint32_t events, void *userdata)
{
	ssize_t readlen = 0;
	struct test_data *data = userdata;
	uint64_t val = 0;
	data->counter++;
	do {
		readlen = read(fd, &val, sizeof(val));
	} while (readlen < 0 && errno == EINTR);
	CU_ASSERT_EQUAL(readlen, sizeof(val));
}

/** */
static void test_loop(int is_epoll)
{
	int res = 0;
	int tfd1 = -1, tfd2 = -1, tfd3 = -1;
	int fd = -1;
	struct test_data data;
	struct pomp_loop *loop = NULL;

	memset(&data, 0, sizeof(data));

	/* Create loop */
	loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(loop);

	/* Create timers for testing */
	tfd1 = setup_timerfd(100, 500);
	CU_ASSERT_TRUE_FATAL(tfd1 >= 0);
	tfd2 = setup_timerfd(50, 500);
	CU_ASSERT_TRUE_FATAL(tfd2 >= 0);
	tfd3 = setup_timerfd(150, 500);
	CU_ASSERT_TRUE_FATAL(tfd3 >= 0);

	/* Add timer in loop */
	res = pomp_loop_add(loop, tfd1, POMP_FD_EVENT_IN, &timerfd_cb, &data);
	CU_ASSERT_EQUAL(res, 0);

	res = pomp_loop_has_fd(loop, tfd1);
	CU_ASSERT_EQUAL(res, 1);

	res = pomp_loop_has_fd(loop, tfd2);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid add (already in loop) */
	res = pomp_loop_add(loop, tfd1, POMP_FD_EVENT_IN, &timerfd_cb, &data);
	CU_ASSERT_EQUAL(res, -EEXIST);

	/* Invalid add (NULL param) */
	res = pomp_loop_add(NULL, tfd1, POMP_FD_EVENT_IN, &timerfd_cb, &data);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_loop_add(loop, tfd1, POMP_FD_EVENT_IN, NULL, &data);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid add (invalid events) */
	res = pomp_loop_add(loop, tfd1, 0, &timerfd_cb, &data);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid add (invalid fd) */
	res = pomp_loop_add(loop, -1, POMP_FD_EVENT_IN, &timerfd_cb, &data);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Update events */
	res = pomp_loop_update(loop, tfd1, 0);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_update(loop, tfd1, POMP_FD_EVENT_IN | POMP_FD_EVENT_OUT);
	CU_ASSERT_EQUAL(res, 0);

	/* Update events */
	res = pomp_loop_update2(loop, tfd1, 0, POMP_FD_EVENT_OUT);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_update2(loop, tfd1, POMP_FD_EVENT_IN, 0);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid update (NULL param) */
	res = pomp_loop_update(NULL, tfd1, POMP_FD_EVENT_IN | POMP_FD_EVENT_OUT);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid update (invalid fd) */
	res = pomp_loop_update(loop, -1, POMP_FD_EVENT_IN | POMP_FD_EVENT_OUT);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid remove (fd not registered) */
	res = pomp_loop_update(loop, 2, POMP_FD_EVENT_IN | POMP_FD_EVENT_OUT);
	CU_ASSERT_EQUAL(res, -ENOENT);

	/* Update again events */
	res = pomp_loop_update(loop, tfd1, POMP_FD_EVENT_IN);
	CU_ASSERT_EQUAL(res, 0);

	/* Add 2nd and 3rd timer in loop */
	res = pomp_loop_add(loop, tfd2, POMP_FD_EVENT_IN, &timerfd_cb, &data);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_add(loop, tfd3, POMP_FD_EVENT_IN, &timerfd_cb, &data);
	CU_ASSERT_EQUAL(res, 0);

	/* Get loop fd */
	fd = pomp_loop_get_fd(loop);
	CU_ASSERT_TRUE((is_epoll && fd >= 0) || (!is_epoll && fd == -ENOSYS));
	fd = pomp_loop_get_fd(NULL);
	CU_ASSERT_EQUAL(fd, -EINVAL);

	/* Run loop with different timeout first one should have all timers) */
	res = pomp_loop_wait_and_process(loop, 500);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_wait_and_process(loop, 0);
	CU_ASSERT_TRUE(res == -ETIMEDOUT || res == 0);
	res = pomp_loop_wait_and_process(loop, -1);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid run (NULL param) */
	res = pomp_loop_wait_and_process(NULL, 0);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid destroy (NULL param) */
	res = pomp_loop_destroy(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid destroy (busy) */
	res = pomp_loop_destroy(loop);
	CU_ASSERT_EQUAL(res, -EBUSY);

	/* Invalid remove (NULL param) */
	res = pomp_loop_remove(NULL, tfd1);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid remove (invalid fd) */
	res = pomp_loop_remove(loop, -1);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid remove (fd not registered) */
	res = pomp_loop_remove(loop, 2);
	CU_ASSERT_EQUAL(res, -ENOENT);

	/* Remove timers */
	res = pomp_loop_remove(loop, tfd1);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_remove(loop, tfd2);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_remove(loop, tfd3);
	CU_ASSERT_EQUAL(res, 0);

	/* Close timers */
	res = close(tfd1);
	CU_ASSERT_EQUAL(res, 0);
	res = close(tfd2);
	CU_ASSERT_EQUAL(res, 0);
	res = close(tfd3);
	CU_ASSERT_EQUAL(res, 0);

	/* Destroy loop */
	res = pomp_loop_destroy(loop);
	CU_ASSERT_EQUAL(res, 0);
}

#endif /* __linux__ */

#if defined(__FreeBSD__) || defined(__APPLE__)
static void test_loop(int is_epoll)
{
	/* TODO */
}
#endif /* __FreeBSD__ || __APPLE__ */

#ifndef _WIN32

/** */
static void *test_loop_wakeup_thread(void *arg)
{
	int res = 0, i = 0;
	struct pomp_loop *loop = arg;

	for (i = 0; i < 10; i++) {
		usleep(100 * 1000);
		res = pomp_loop_wakeup(loop);
		CU_ASSERT_EQUAL(res, 0);
	}

	return NULL;
}

/** */
static void test_loop_wakeup(void)
{
	int res = 0, i = 0;
	struct pomp_loop *loop = NULL;
	pthread_t thread;

	/* Create loop */
	loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(loop);

	/* Create a thread that will do the wakeup */
	res = pthread_create(&thread, NULL, &test_loop_wakeup_thread, loop);
	CU_ASSERT_EQUAL(res, 0);

	for (i = 0; i < 10; i++) {
		/* Execute loop until wakeup, shall not timeout */
		res = pomp_loop_wait_and_process(loop, 1000);
		CU_ASSERT_EQUAL(res, 0);
	}

	res = pthread_join(thread, NULL);
	CU_ASSERT_EQUAL(res, 0);

	res = pomp_loop_wakeup(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Destroy loop */
	res = pomp_loop_destroy(loop);
	CU_ASSERT_EQUAL(res, 0);
}

#endif /* !_WIN32 */

#ifdef _WIN32

/** */
static HANDLE setup_timer_win32(uint32_t delay, uint32_t period)
{
	int res = 0;
	HANDLE htimer = NULL;
	LARGE_INTEGER tval;

	htimer = CreateWaitableTimer(NULL, 0, NULL);
	CU_ASSERT_PTR_NOT_NULL_FATAL(htimer);

	/* Setup timeout, convert ms to 100th of ns */
	tval.QuadPart = - (LONGLONG)delay * 1000 * 10;
	res = SetWaitableTimer(htimer, &tval, period, NULL, NULL, 0);
	CU_ASSERT_EQUAL_FATAL(res, 1);

	return htimer;
}

/** */
static void timer_win32_cb(int fd, uint32_t events, void *userdata)
{
	struct test_data *data = userdata;
	data->counter++;
}

/** */
static void test_loop(void)
{
	int res = 0;
	HANDLE htimer1 = NULL, htimer2 = NULL, htimer3 = NULL;
	struct test_data data;
	struct pomp_loop *loop = NULL;
	struct pomp_fd *pfd = NULL;

	memset(&data, 0, sizeof(data));

	/* Create loop */
	loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(loop);

	/* Create timers for testing */
	htimer1 = setup_timer_win32(100, 500);
	CU_ASSERT_PTR_NOT_NULL_FATAL(htimer1);
	htimer2 = setup_timer_win32(50, 500);
	CU_ASSERT_PTR_NOT_NULL_FATAL(htimer2);
	htimer3 = setup_timer_win32(150, 500);
	CU_ASSERT_PTR_NOT_NULL_FATAL(htimer3);

	/* Add timer in loop */
	pfd = pomp_loop_win32_add_pfd_with_hevt(loop, htimer1, &timer_win32_cb, &data);
	CU_ASSERT_PTR_NOT_NULL(pfd);

	/* Add 2nd and 3rd timer in loop */
	pfd = pomp_loop_win32_add_pfd_with_hevt(loop, htimer2, &timer_win32_cb, &data);
	CU_ASSERT_PTR_NOT_NULL(pfd);
	pfd = pomp_loop_win32_add_pfd_with_hevt(loop, htimer3, &timer_win32_cb, &data);
	CU_ASSERT_PTR_NOT_NULL(pfd);

	/* Run loop with different timeout first one should have all timers) */
	res = pomp_loop_wait_and_process(loop, 500);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_wait_and_process(loop, 0);
	CU_ASSERT_TRUE(res == -ETIMEDOUT || res == 0);
	res = pomp_loop_wait_and_process(loop, -1);
	CU_ASSERT_EQUAL(res, 0);

	/* Invalid run (NULL param) */
	res = pomp_loop_wait_and_process(NULL, 0);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid destroy (NULL param) */
	res = pomp_loop_destroy(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Invalid destroy (busy) */
	res = pomp_loop_destroy(loop);
	CU_ASSERT_EQUAL(res, -EBUSY);

	/* Remove timers */
	pfd = pomp_loop_win32_find_pfd_by_hevt(loop, htimer1);
	CU_ASSERT_PTR_NOT_NULL(pfd);
	res = pomp_loop_remove_pfd(loop, pfd);
	CU_ASSERT_EQUAL(res, 0);
	free(pfd);
	pfd = pomp_loop_win32_find_pfd_by_hevt(loop, htimer2);
	CU_ASSERT_PTR_NOT_NULL(pfd);
	res = pomp_loop_remove_pfd(loop, pfd);
	CU_ASSERT_EQUAL(res, 0);
	free(pfd);
	pfd = pomp_loop_win32_find_pfd_by_hevt(loop, htimer3);
	CU_ASSERT_PTR_NOT_NULL(pfd);
	res = pomp_loop_remove_pfd(loop, pfd);
	CU_ASSERT_EQUAL(res, 0);
	free(pfd);

	/* Close timers */
	res = CloseHandle(htimer1);
	CU_ASSERT_EQUAL(res, 1);
	res = CloseHandle(htimer2);
	CU_ASSERT_EQUAL(res, 1);
	res = CloseHandle(htimer3);
	CU_ASSERT_EQUAL(res, 1);

	/* Destroy loop */
	res = pomp_loop_destroy(loop);
	CU_ASSERT_EQUAL(res, 0);
}

/** */
static DWORD WINAPI test_loop_wakeup_thread(void *arg)
{
	int res = 0, i = 0;
	struct pomp_loop *loop = arg;

	for (i = 0; i < 10; i++) {
		usleep(100 * 1000);
		res = pomp_loop_wakeup(loop);
		CU_ASSERT_EQUAL(res, 0);
	}

	return 0;
}

/** */
static void test_loop_wakeup(void)
{
	int res = 0, i = 0;
	struct pomp_loop *loop = NULL;
	HANDLE hthread = NULL;
	DWORD threadid = 0;

	/* Create loop */
	loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(loop);

	/* Create a thread that will do the wakeup */
	hthread = CreateThread(NULL, 0, &test_loop_wakeup_thread, loop, 0, &threadid);
	CU_ASSERT_PTR_NOT_NULL_FATAL(hthread);

	for (i = 0; i < 10; i++) {
		/* Execute loop until wakeup, shall not timeout */
		res = pomp_loop_wait_and_process(loop, 1000);
		CU_ASSERT_EQUAL(res, 0);
	}

	res = WaitForSingleObject(hthread, INFINITE);
	CU_ASSERT_EQUAL(res, WAIT_OBJECT_0);
	CloseHandle(hthread);

	res = pomp_loop_wakeup(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Destroy loop */
	res = pomp_loop_destroy(loop);
	CU_ASSERT_EQUAL(res, 0);
}

#endif /* _WIN32 */

/** */
struct idle_data {
	struct pomp_loop  *loop;
	int               n;
	int               recursion_enabled;
	int               isdestroying;
};

/** */
static void idle_cb(void *userdata)
{
	int res = 0;
	struct idle_data *data = userdata;
	data->n++;

	if (data->recursion_enabled) {
		/* Check recursive idle add */
		res = pomp_loop_idle_add(data->loop, &idle_cb, data);
		if (data->isdestroying) {
			CU_ASSERT_EQUAL(res, -EPERM);
		} else {
			CU_ASSERT_EQUAL(res, 0);
		}
		data->recursion_enabled = 0;
	}
}

/** */
static void test_loop_idle(void)
{
	int res = 0;
	struct idle_data data = {NULL, 0, 0, 0};

	/* Create loop */
	data.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(data.loop);

	/* Check register function is called once */
	data.n = 0;
	res = pomp_loop_idle_add(data.loop, &idle_cb, &data);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_process_fd(data.loop);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(data.n, 1);
	res = pomp_loop_process_fd(data.loop);
	CU_ASSERT_EQUAL(res, -ETIMEDOUT);
	CU_ASSERT_EQUAL(data.n, 1);

	/* Check register function is called twice */
	data.n = 0;
	data.recursion_enabled = 1;
	res = pomp_loop_idle_add(data.loop, &idle_cb, &data);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_idle_add(data.loop, &idle_cb, &data);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_process_fd(data.loop);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(data.n, 2);
	/* Check register function is called by the recursive idle add */
	res = pomp_loop_process_fd(data.loop);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(data.n, 3);

	/* Check register function is not called */
	data.n = 0;
	res = pomp_loop_idle_add(data.loop, &idle_cb, &data);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_idle_remove(data.loop, &idle_cb, &data);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_wait_and_process(data.loop, 0);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(data.n, 0);

	res = pomp_loop_idle_add(NULL, &idle_cb, &data);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_loop_idle_add(data.loop, NULL, &data);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_loop_idle_remove(NULL, &idle_cb, &data);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Check register function is called by the destroy of the loop */
	data.n = 0;
	data.recursion_enabled = 1;
	data.isdestroying = 1;
	res = pomp_loop_idle_add(data.loop, &idle_cb, &data);
	CU_ASSERT_EQUAL(res, 0);
	/* Destroy loop */
	res = pomp_loop_destroy(data.loop);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(data.n, 1);
}

/** */
#ifdef POMP_HAVE_LOOP_EPOLL
static void test_loop_epoll(void)
{
	const struct pomp_loop_ops *loop_ops = NULL;
	loop_ops = pomp_loop_set_ops(&pomp_loop_epoll_ops);
	test_loop(1);
	test_loop_wakeup();
	test_loop_idle();
	pomp_loop_set_ops(loop_ops);
}
#endif /* POMP_HAVE_LOOP_EPOLL */

/** */
#ifdef POMP_HAVE_LOOP_POLL
static void test_loop_poll(void)
{
	const struct pomp_loop_ops *loop_ops = NULL;
	loop_ops = pomp_loop_set_ops(&pomp_loop_poll_ops);
	test_loop(0);
	test_loop_wakeup();
	test_loop_idle();
	pomp_loop_set_ops(loop_ops);
}
#endif /* POMP_HAVE_LOOP_POLL */

/** */
#ifdef POMP_HAVE_LOOP_WIN32
static void test_loop_win32(void)
{
	const struct pomp_loop_ops *loop_ops = NULL;
	loop_ops = pomp_loop_set_ops(&pomp_loop_win32_ops);
	test_loop();
	test_loop_wakeup();
	test_loop_idle();
	pomp_loop_set_ops(loop_ops);
}
#endif /* POMP_HAVE_LOOP_WIN32 */

/* Disable some gcc warnings for test suite descriptions */
#ifdef __GNUC__
#  pragma GCC diagnostic ignored "-Wcast-qual"
#endif /* __GNUC__ */

/** */
static CU_TestInfo s_loop_tests[] = {

#ifdef POMP_HAVE_LOOP_EPOLL
	{(char *)"epoll", &test_loop_epoll},
#endif /* POMP_HAVE_LOOP_EPOLL */

#ifdef POMP_HAVE_LOOP_POLL
	{(char *)"poll", &test_loop_poll},
#endif /* POMP_HAVE_LOOP_POLL */

#ifdef POMP_HAVE_LOOP_WIN32
	{(char *)"win32", &test_loop_win32},
#endif /* POMP_HAVE_LOOP_WIN32 */

	CU_TEST_INFO_NULL,
};

/** */
/*extern*/ CU_SuiteInfo g_suites_loop[] = {
	{(char *)"loop", NULL, NULL, s_loop_tests},
	CU_SUITE_INFO_NULL,
};
