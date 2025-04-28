/**
 * @file pomp_loop_sync.c
 *
 * @brief Loop thread synchronisation.
 *
 * @author yves-marie.morgan@parrot.com
 *
 * Copyright (c) 2023 Parrot Drones SAS.
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

#include "pomp_priv.h"

#ifdef POMP_HAVE_LOOP_SYNC

int pomp_loop_sync_init(struct pomp_loop_sync *sync, struct pomp_loop *loop)
{
	POMP_RETURN_ERR_IF_FAILED(sync != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(loop != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(sync->loop == NULL, -EBUSY);
	sync->loop = loop;
	pthread_mutex_init(&sync->mutex, NULL);
	pthread_cond_init(&sync->cond_count, NULL);
	pthread_cond_init(&sync->cond_waiters, NULL);
	return 0;
}

int pomp_loop_sync_clear(struct pomp_loop_sync *sync)
{
	POMP_RETURN_ERR_IF_FAILED(sync != NULL, -EINVAL);
	if (sync->loop != NULL) {
		pthread_mutex_destroy(&sync->mutex);
		pthread_cond_destroy(&sync->cond_count);
		pthread_cond_destroy(&sync->cond_waiters);
		memset(sync, 0, sizeof(*sync));
	}
	return 0;
}

int pomp_loop_sync_lock(struct pomp_loop_sync *sync, int willblock)
{
	POMP_RETURN_ERR_IF_FAILED(sync != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(sync->loop != NULL, -EINVAL);

	pthread_mutex_lock(&sync->mutex);

	/* If current thread is not the owner, wait */
	if (!pthread_equal(sync->owner, pthread_self())) {
		sync->waiters++;

		/* If the acquire is for the blocking processing loop,
		 * do not acquire it now if there is other waiters */
		if (willblock) {
			while (sync->waiters > 1) {
				pthread_cond_wait(&sync->cond_waiters,
						&sync->mutex);
			}
		}

		/* Wait until loop can be acquired */
		while (sync->count != 0) {
			pomp_loop_wakeup(sync->loop);
			pthread_cond_wait(&sync->cond_count, &sync->mutex);
		}

		sync->waiters--;
		if (sync->waiters <= 1)
			pthread_cond_signal(&sync->cond_waiters);
	}

	/* OK, we are owner of loop */
	sync->owner = pthread_self();
	sync->count++;
	pthread_mutex_unlock(&sync->mutex);

	return 0;
}

int pomp_loop_sync_unlock(struct pomp_loop_sync *sync)
{
	int res = 0;
	POMP_RETURN_ERR_IF_FAILED(sync != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(sync->loop != NULL, -EINVAL);

	pthread_mutex_lock(&sync->mutex);

	if (!pthread_equal(sync->owner, pthread_self())) {
		res = -EPERM;
		POMP_LOGE("Thread does not own the loop");
	} else if (--sync->count == 0) {
		memset(&sync->owner, 0, sizeof(sync->owner));
		pthread_cond_signal(&sync->cond_count);
	}

	pthread_mutex_unlock(&sync->mutex);
	return res;
}

#else /* !POMP_HAVE_LOOP_SYNC */

int pomp_loop_sync_init(struct pomp_loop_sync *sync, struct pomp_loop *loop)
{
	return -ENOSYS;
}

int pomp_loop_sync_clear(struct pomp_loop_sync *sync)
{
	return -ENOSYS;
}

int pomp_loop_sync_lock(struct pomp_loop_sync *sync, int willblock)
{
	return -ENOSYS;
}

int pomp_loop_sync_unlock(struct pomp_loop_sync *sync)
{
	return -ENOSYS;
}

#endif /* !POMP_HAVE_LOOP_SYNC */
