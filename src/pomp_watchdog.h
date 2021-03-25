/**
 * @file pomp_watchdog.h
 *
 * @brief Loop watchdog.
 *
 * @author yves-marie.morgan@parrot.com
 *
 * Copyright (c) 2021 Parrot Donres SAS.
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

#ifndef _POMP_WATCHDOG_H_
#define _POMP_WATCHDOG_H_

struct pomp_watchdog {
	pthread_mutex_t		mutex;
	pthread_cond_t		cond;
	pthread_t		thread;
	int			started;
	int			should_stop;

	struct pomp_loop	*loop;
	uint32_t		delay;
	pomp_watchdog_cb_t	cb;
	void			*userdata;

	uint32_t		counter;
	int			monitoring;
	struct timespec		next_timeout;
};

void pomp_watchdog_init(struct pomp_watchdog *watchdog);

void pomp_watchdog_clear(struct pomp_watchdog *watchdog);

int pomp_watchdog_start(struct pomp_watchdog *watchdog,
	struct pomp_loop *loop,
	uint32_t delay,
	pomp_watchdog_cb_t cb,
	void *userdata);

int pomp_watchdog_stop(struct pomp_watchdog *watchdog);

void pomp_watchdog_enter(struct pomp_watchdog *watchdog);

void pomp_watchdog_leave(struct pomp_watchdog *watchdog);

#endif /* !_POMP_WATCHDOG_H_ */
