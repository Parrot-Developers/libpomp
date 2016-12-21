/**
 * @file pomp_log.h
 *
 * @brief Log functions and macros.
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

#ifndef _POMP_LOG_H_
#define _POMP_LOG_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Define POMP_LOG_API_ERR to enable log when API is not correctly called */
/*#define POMP_LOG_API_ERR*/

#if defined(BUILD_LIBULOG)

#define ULOG_TAG pomp
#include "ulog.h"

/** Log as debug */
#define POMP_LOGD(_fmt, ...)	ULOGD(_fmt, ##__VA_ARGS__)
/** Log as info */
#define POMP_LOGI(_fmt, ...)	ULOGI(_fmt, ##__VA_ARGS__)
/** Log as warning */
#define POMP_LOGW(_fmt, ...)	ULOGW(_fmt, ##__VA_ARGS__)
/** Log as error */
#define POMP_LOGE(_fmt, ...)	ULOGE(_fmt, ##__VA_ARGS__)

#elif defined(ANDROID)

#define LOG_TAG "pomp"
#ifdef ANDROID_NDK

#include <android/log.h>
/** Log as debug */
#define POMP_LOGD(_fmt, ...) \
	__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, _fmt, ##__VA_ARGS__)
/** Log as info */
#define POMP_LOGI(_fmt, ...) \
	__android_log_print(ANDROID_LOG_INFO, LOG_TAG, _fmt, ##__VA_ARGS__)
/** Log as warning */
#define POMP_LOGW(_fmt, ...) \
	__android_log_print(ANDROID_LOG_WARN, LOG_TAG, _fmt, ##__VA_ARGS__)
/** Log as error */
#define POMP_LOGE(_fmt, ...) \
	__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, _fmt, ##__VA_ARGS__)

#else

#include "cutils/log.h"

#ifndef ALOGD
#define ALOGD LOGD
#define ALOGI LOGI
#define ALOGW LOGW
#define ALOGE LOGE
#endif

/** Log as debug */
#define POMP_LOGD(_fmt, ...)	ALOGD(_fmt, ##__VA_ARGS__)
/** Log as info */
#define POMP_LOGI(_fmt, ...)	ALOGI(_fmt, ##__VA_ARGS__)
/** Log as warning */
#define POMP_LOGW(_fmt, ...)	ALOGW(_fmt, ##__VA_ARGS__)
/** Log as error */
#define POMP_LOGE(_fmt, ...)	ALOGE(_fmt, ##__VA_ARGS__)

#endif

#else /* !BUILD_LIBULOG && !ANDROID */

/** Generic log */
#define POMP_LOG(_fmt, ...)	fprintf(stderr, _fmt "\n", ##__VA_ARGS__)
/** Log as debug */
#define POMP_LOGD(_fmt, ...)	POMP_LOG("[D]" _fmt, ##__VA_ARGS__)
/** Log as info */
#define POMP_LOGI(_fmt, ...)	POMP_LOG("[I]" _fmt, ##__VA_ARGS__)
/** Log as warning */
#define POMP_LOGW(_fmt, ...)	POMP_LOG("[W]" _fmt, ##__VA_ARGS__)
/** Log as error */
#define POMP_LOGE(_fmt, ...)	POMP_LOG("[E]" _fmt, ##__VA_ARGS__)

#endif /* !BUILD_LIBULOG */

/** Log error with errno */
#define POMP_LOG_ERRNO(_func) \
	POMP_LOGE("%s err=%d(%s)", _func, errno, strerror(errno))

/** Log error with fd and errno */
#define POMP_LOG_FD_ERRNO(_func, _fd) \
	POMP_LOGE("%s(fd=%d) err=%d(%s)", _func, _fd, errno, strerror(errno))

#ifdef POMP_LOG_API_ERR

/** Log error if condition failed and return from function */
#define POMP_RETURN_IF_FAILED(_cond, _err) \
	do { \
		if (!(_cond)) { \
			POMP_LOGE("%s:%d err=%d(%s)", __func__, __LINE__, \
					(_err), strerror(-(_err))); \
			return; \
		} \
	} while (0)

/** Log error if condition failed and return error from function */
#define POMP_RETURN_ERR_IF_FAILED(_cond, _err) \
	do { \
		if (!(_cond)) { \
			POMP_LOGE("%s:%d err=%d(%s)", __func__, __LINE__, \
					(_err), strerror(-(_err))); \
			/* codecheck_ignore[RETURN_PARENTHESES] */ \
			return (_err); \
		} \
	} while (0)

/** Log error if condition failed and return value from function */
#define POMP_RETURN_VAL_IF_FAILED(_cond, _err, _val) \
	do { \
		if (!(_cond)) { \
			POMP_LOGE("%s:%d err=%d(%s)", __func__, __LINE__, \
					(_err), strerror(-(_err))); \
			/* codecheck_ignore[RETURN_PARENTHESES] */ \
			return (_val); \
		} \
	} while (0)

#else /* ! POMP_LOG_API_ERR */

/** Log error if condition failed and return from function */
#define POMP_RETURN_IF_FAILED(_cond, _err) \
	do { \
		if (!(_cond)) { \
			return; \
		} \
	} while (0)

/** Log error if condition failed and return error from function */
#define POMP_RETURN_ERR_IF_FAILED(_cond, _err) \
	do { \
		if (!(_cond)) { \
			/* codecheck_ignore[RETURN_PARENTHESES] */ \
			return (_err); \
		} \
	} while (0)

/** Log error if condition failed and return value from function */
#define POMP_RETURN_VAL_IF_FAILED(_cond, _err, _val) \
	do { \
		if (!(_cond)) { \
			/* codecheck_ignore[RETURN_PARENTHESES] */ \
			return (_val); \
		} \
	} while (0)
#endif /* !POMP_LOG_API_ERR */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_POMP_LOG_H_ */
