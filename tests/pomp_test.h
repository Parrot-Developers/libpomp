/**
 * @file pomp_test.h
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

#ifndef _POMP_TEST_H_
#define _POMP_TEST_H_

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <signal.h>
#include <inttypes.h>
#include <stddef.h>

#include <sys/stat.h>

#ifndef _WIN32
#include <pthread.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif /* !_WIN32 */

#include "libpomp.h"
#include "pomp_priv.h"

#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include <CUnit/Automated.h>

/**
 */
extern CU_SuiteInfo g_suites_basic[];
extern CU_SuiteInfo g_suites_addr[];
extern CU_SuiteInfo g_suites_loop[];
extern CU_SuiteInfo g_suites_timer[];
extern CU_SuiteInfo g_suites_ctx[];
extern CU_SuiteInfo g_suites_ipc[];

#endif /* !_POMP_TEST_H_ */
