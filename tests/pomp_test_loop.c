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
static void timer_win32_cb(struct pomp_timer *timer, void *userdata)
{
	struct test_data *data = userdata;
	data->counter++;
}

/** */
static struct pomp_timer *setup_timer_win32(struct pomp_loop *loop,
		uint32_t delay, uint32_t period,
		struct test_data *data)
{
	int res = 0;
	struct pomp_timer *timer = NULL;

	timer = pomp_timer_new(loop, &timer_win32_cb, data);
	CU_ASSERT_PTR_NOT_NULL_FATAL(timer);

	res = pomp_timer_set_periodic(timer, delay, period);
	CU_ASSERT_EQUAL(res, 0);

	return timer;
}

/** */
static void test_loop(void)
{
	int res = 0;
	struct pomp_timer *timer1 = NULL, *timer2 = NULL, *timer3 = NULL;
	struct test_data data;
	struct pomp_loop *loop = NULL;
	HANDLE hevt = NULL;

	memset(&data, 0, sizeof(data));

	/* Create loop */
	loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(loop);

	/* Create timers for testing */
	timer1 = setup_timer_win32(loop, 100, 500, &data);
	CU_ASSERT_PTR_NOT_NULL_FATAL(timer1);
	timer2 = setup_timer_win32(loop, 50, 500, &data);
	CU_ASSERT_PTR_NOT_NULL_FATAL(timer2);
	timer3 = setup_timer_win32(loop, 150, 500, &data);
	CU_ASSERT_PTR_NOT_NULL_FATAL(timer3);

	/* Get the event handle of the loop */
	hevt = (HANDLE)pomp_loop_get_fd(NULL);
	CU_ASSERT_PTR_EQUAL(hevt, (HANDLE)-EINVAL);
	hevt = (HANDLE)pomp_loop_get_fd(loop);
	CU_ASSERT_PTR_NOT_NULL(hevt);

	/* Run loop with different timeout (first one should have all timers) */
	res = WaitForSingleObject(hevt, 500);
	CU_ASSERT_EQUAL(res, WAIT_OBJECT_0);
	res = pomp_loop_wait_and_process(loop, 0);
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

	/* Delete timers */
	res = pomp_timer_destroy(timer1);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_timer_destroy(timer2);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_timer_destroy(timer3);
	CU_ASSERT_EQUAL(res, 0);

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
	int               recursion_cnt;
	int               recursion_rm_cnt;
	int               isdestroying;
};

/** */
static void idle_cb(void *userdata)
{
	int res = 0;
	struct idle_data *data = userdata;
	data->n++;

	if (data->recursion_cnt > 0) {
		/* Check recursive idle add */
		res = pomp_loop_idle_add(data->loop, &idle_cb, data);
		if (data->isdestroying) {
			CU_ASSERT_EQUAL(res, -EPERM);
		} else {
			CU_ASSERT_EQUAL(res, 0);
		}
		data->recursion_cnt --;
	}

	if (data->recursion_rm_cnt > 0) {
		/* Check remove idle in idle */
		res = pomp_loop_idle_remove(data->loop, &idle_cb, data);
		CU_ASSERT_EQUAL(res, 0);
		data->recursion_rm_cnt --;
	}
}

static __attribute__((noinline)) void idle_cb1(void *userdata)
{
	idle_cb(userdata);
}

static __attribute__((noinline)) void idle_cb2(void *userdata)
{
	idle_cb(userdata);
}

/** */
static void test_loop_idle(void)
{
	int res = 0;
	struct idle_data data = {NULL, 0, 0, 0, 0};
	struct idle_data data1 = {NULL, 0, 0, 0, 0};
	struct idle_data data2 = {NULL, 0, 0, 0, 0};

	/* Create loop */
	data.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(data.loop);

	/* Check register function is called once */
	data.n = 0;
	data.recursion_cnt = 0;
	res = pomp_loop_idle_add(data.loop, &idle_cb, &data);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_process_fd(data.loop);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(data.n, 1);
	res = pomp_loop_process_fd(data.loop);
	CU_ASSERT_EQUAL(res, -ETIMEDOUT);
	CU_ASSERT_EQUAL(data.n, 1);

	/* Check register function is called twice in two process */
	data.n = 0;
	data.recursion_cnt = 0;
	res = pomp_loop_idle_add(data.loop, &idle_cb, &data);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_idle_add(data.loop, &idle_cb, &data);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_process_fd(data.loop);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(data.n, 1);
	res = pomp_loop_process_fd(data.loop);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(data.n, 2);

	/* Check register function is called by the recursive idle add
	   by two call of pomp_loop_process_fd() */
	data.n = 0;
	data.recursion_cnt = 1;
	res = pomp_loop_idle_add(data.loop, &idle_cb, &data);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_process_fd(data.loop);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(data.n, 1);
	res = pomp_loop_process_fd(data.loop);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(data.n, 2);

	/* Check register function is not called */
	data.n = 0;
	res = pomp_loop_idle_add(data.loop, &idle_cb, &data);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_idle_add(data.loop, &idle_cb, &data);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_idle_remove(data.loop, &idle_cb, &data);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_wait_and_process(data.loop, 0);
	CU_ASSERT_EQUAL(res, -ETIMEDOUT);
	CU_ASSERT_EQUAL(data.n, 0);

	/* Check remove idle in idle */
	data.n = 0;
	data.recursion_cnt = 0;
	data.recursion_rm_cnt = 1;
	res = pomp_loop_idle_add(data.loop, &idle_cb, &data);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_idle_add(data.loop, &idle_cb, &data);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_wait_and_process(data.loop, 0);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(data.recursion_rm_cnt, 0);
	CU_ASSERT_EQUAL(data.n, 1);
	res = pomp_loop_wait_and_process(data.loop, 0);
	CU_ASSERT_EQUAL(res, -ETIMEDOUT);

	data1 = data;
	data2 = data;

	data1.n = 0;
	data1.recursion_cnt = 0;
	data1.recursion_rm_cnt = 0;
	data2.n = 0;
	data2.recursion_cnt = 0;
	data2.recursion_rm_cnt = 0;
	res = pomp_loop_idle_add(data.loop, &idle_cb1, &data1);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_idle_add(data.loop, &idle_cb2, &data2);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_wait_and_process(data.loop, 0);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_wait_and_process(data.loop, 0);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(data1.n, 1);
	CU_ASSERT_EQUAL(data2.n, 1);

	/* Check pomp_loop_idle_add_with_cookie */
	data1.n = 0;
	data1.recursion_cnt = 0;
	data1.recursion_rm_cnt = 0;
	data2.n = 0;
	data2.recursion_cnt = 0;
	data2.recursion_rm_cnt = 0;
	res = pomp_loop_idle_add_with_cookie(data.loop, &idle_cb1, &data1, &data1.n);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_idle_add_with_cookie(data.loop, &idle_cb2, &data2, &data2.n);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_wait_and_process(data.loop, 0);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(data1.n, 1);
	CU_ASSERT_EQUAL(data2.n, 0);
	res = pomp_loop_wait_and_process(data.loop, 0);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(data1.n, 1);
	CU_ASSERT_EQUAL(data2.n, 1);
	res = pomp_loop_wait_and_process(data.loop, 0);
	CU_ASSERT_EQUAL(res, -ETIMEDOUT);
	CU_ASSERT_EQUAL(data1.n, 1);
	CU_ASSERT_EQUAL(data2.n, 1);

	/* Check pomp_loop_idle_remove_by_cookie */
	data1.n = 0;
	data1.recursion_cnt = 0;
	data1.recursion_rm_cnt = 0;
	data2.n = 0;
	data2.recursion_cnt = 0;
	data2.recursion_rm_cnt = 0;
	res = pomp_loop_idle_add_with_cookie(data.loop, &idle_cb1, &data1, &data1.n);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_idle_add_with_cookie(data.loop, &idle_cb2, &data2, &data2.n);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_idle_remove_by_cookie(data.loop, &data1.n);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_wait_and_process(data.loop, 0);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(data1.n, 0);
	CU_ASSERT_EQUAL(data2.n, 1);
	res = pomp_loop_idle_remove_by_cookie(data.loop, &data2.n);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_wait_and_process(data.loop, 0);
	CU_ASSERT_EQUAL(res, -ETIMEDOUT);
	CU_ASSERT_EQUAL(data1.n, 0);
	CU_ASSERT_EQUAL(data2.n, 1);

	/* Check pomp_loop_idle_flush */
	data1.n = 0;
	data1.recursion_cnt = 0;
	data1.recursion_rm_cnt = 0;
	data2.n = 0;
	data2.recursion_cnt = 0;
	data2.recursion_rm_cnt = 0;
	res = pomp_loop_idle_add(data.loop, &idle_cb1, &data1);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_idle_add(data.loop, &idle_cb2, &data2);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_idle_flush(data.loop);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(data1.n, 1);
	CU_ASSERT_EQUAL(data2.n, 1);
	res = pomp_loop_wait_and_process(data.loop, 0);
	CU_ASSERT_EQUAL(res, -ETIMEDOUT);

	/* Check pomp_loop_idle_flush_by_cookie */
	data1.n = 0;
	data1.recursion_cnt = 0;
	data1.recursion_rm_cnt = 0;
	data2.n = 0;
	data2.recursion_cnt = 0;
	data2.recursion_rm_cnt = 0;
	res = pomp_loop_idle_add_with_cookie(data.loop, &idle_cb1, &data1, &data1.n);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_idle_add_with_cookie(data.loop, &idle_cb2, &data2, &data2.n);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_idle_flush_by_cookie(data.loop, &data1.n);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(data1.n, 1);
	CU_ASSERT_EQUAL(data2.n, 0);
	res = pomp_loop_idle_remove_by_cookie(data.loop, &data2.n);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_wait_and_process(data.loop, 0);
	CU_ASSERT_EQUAL(res, -ETIMEDOUT);
	CU_ASSERT_EQUAL(data1.n, 1);
	CU_ASSERT_EQUAL(data2.n, 0);

	/* Check invalid parameters */
	res = pomp_loop_idle_add(NULL, &idle_cb, &data);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_loop_idle_add(data.loop, NULL, &data);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_loop_idle_add_with_cookie(NULL, &idle_cb, &data, &data.n);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_loop_idle_add_with_cookie(data.loop, NULL, &data, &data.n);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_loop_idle_add_with_cookie(data.loop, &idle_cb, &data, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_loop_idle_remove(NULL, &idle_cb, &data);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_loop_idle_remove_by_cookie(NULL, &data.n);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_loop_idle_remove_by_cookie(data.loop, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_loop_idle_flush(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_loop_idle_flush_by_cookie(NULL, &data.n);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = pomp_loop_idle_flush_by_cookie(data.loop, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* Idle cb not called during destroy, and EBUSY returned */
	data.n = 0;
	data.recursion_cnt = 0;
	data.recursion_rm_cnt = 0;
	res = pomp_loop_idle_add(data.loop, &idle_cb, &data);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_destroy(data.loop);
	CU_ASSERT_EQUAL(res, -EBUSY);
	CU_ASSERT_EQUAL(data.n, 0);
	res = pomp_loop_idle_remove(data.loop, &idle_cb, &data);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_destroy(data.loop);
	CU_ASSERT_EQUAL(res, 0);
}

#ifdef POMP_HAVE_WATCHDOG

/** */
struct watchdog_data {
	struct pomp_loop  *loop;
	int               sleep_value;
	int               expired;
};

static void watchdog_cb(struct pomp_loop *loop, void *userdata)
{
	struct watchdog_data *data = userdata;
	data->expired++;
}

static void watchdog_evt_cb(struct pomp_evt *evt, void *userdata)
{
	struct watchdog_data *data = userdata;
	if (data->sleep_value > 0)
		usleep(data->sleep_value * 1000);
}

/** */
static void test_loop_watchdog(void)
{
	int res = 0;
	struct watchdog_data data = {NULL, 0, 0};
	struct pomp_evt *evt = NULL;

	/* Create loop and event */
	data.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(data.loop);
	evt = pomp_evt_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(evt);
	res = pomp_evt_attach_to_loop(evt, data.loop, &watchdog_evt_cb, &data);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_watchdog_enable(data.loop, 500, &watchdog_cb, &data);
	CU_ASSERT_EQUAL(res, 0);

	/* Normal processing time */
	data.sleep_value = 100;
	data.expired = 0;
	res = pomp_evt_signal(evt);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_wait_and_process(data.loop, -1);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(data.expired, 0);

	/* Excessive processing time (watchdog should trigger only once) */
	data.sleep_value = 1500;
	data.expired = 0;
	res = pomp_evt_signal(evt);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_wait_and_process(data.loop, -1);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(data.expired, 1);

	/* Disable watchdog */
	res = pomp_loop_watchdog_disable(data.loop);
	CU_ASSERT_EQUAL(res, 0);

	/* Excessive processing time (watchdog should not trigger) */
	data.sleep_value = 1500;
	data.expired = 0;
	res = pomp_evt_signal(evt);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_wait_and_process(data.loop, -1);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(data.expired, 0);

	/* Cleanup */
	res = pomp_evt_detach_from_loop(evt, data.loop);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_evt_destroy(evt);
	CU_ASSERT_EQUAL(res, 0);
	res = pomp_loop_destroy(data.loop);
	CU_ASSERT_EQUAL(res, 0);
}

#endif /* POMP_HAVE_WATCHDOG */

/** */
#ifdef POMP_HAVE_LOOP_EPOLL
static void test_loop_epoll(void)
{
	const struct pomp_loop_ops *loop_ops = NULL;
	loop_ops = pomp_loop_set_ops(&pomp_loop_epoll_ops);
	test_loop(1);
	test_loop_wakeup();
	test_loop_idle();
#ifdef POMP_HAVE_WATCHDOG
	test_loop_watchdog();
#endif /* POMP_HAVE_WATCHDOG */
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
#ifdef POMP_HAVE_WATCHDOG
	test_loop_watchdog();
#endif /* POMP_HAVE_WATCHDOG */
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
#ifdef POMP_HAVE_WATCHDOG
	test_loop_watchdog();
#endif /* POMP_HAVE_WATCHDOG */
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
