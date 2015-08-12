/**
 * @file pomp_test.c
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
 *   * Neither the name of the <organization> nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "pomp_test.h"

#ifndef _WIN32

/** */
static int *lsof(void)
{
	DIR *dir = NULL;
	int *fds = NULL;
	int fd = -1;
	size_t fdcount = 0;
	struct dirent *de = NULL;

	/* Open directory */
	dir = opendir("/proc/self/fd");
	if (dir == NULL)
		return NULL;

	/* Browse directory */
	de = readdir(dir);
	while (de != NULL) {
		if (strcmp(de->d_name, ".") != 0
				&& strcmp(de->d_name, "..") != 0) {
			/* Get fd entry */
			fd = atoi(de->d_name);
			if (fd != dirfd(dir)) {
				/* Add in array */
				fds = realloc(fds, (fdcount + 1) * sizeof(int));
				fds[fdcount++] = fd;
			}
		}
		de = readdir(dir);
	}

	/* Add last entry in array */
	fds = realloc(fds, (fdcount + 1) * sizeof(int));
	fds[fdcount] = -1;

	closedir(dir);
	return fds;
}

/**
 */
static void check_fds(const int *fds1, const int *fds2)
{
	const int *fds = NULL;
	int found = 0;
	char path[32] = "";
	char *rpath = NULL;

	/* For each fd in fds2, make sure it was in fds1 */
	while (*fds2 != -1) {
		found = 0;
		for (fds = fds1; *fds != -1 && !found; fds++) {
			if (*fds == *fds2)
				found = 1;
		}

		if (!found) {
			snprintf(path, sizeof(path), "/proc/self/fd/%d", *fds2);
			rpath = realpath(path, NULL);
			fprintf(stderr, "Leaked fd %d (%s)\n", *fds2, rpath);
			free(rpath);
		}

		/* Check next fd */
		fds2++;
	}
}

#else /* _WIN32 */

static int *lsof(void) {return NULL;}
static void check_fds(const int *fds1, const int *fds2) {}

#endif /* _WIN32 */

/**
 */
int main(int argc, char *argv[])
{
	int *fds1 = NULL;
	int *fds2 = NULL;

#ifdef _WIN32
	/* Initialize winsock API */
	WSADATA wsadata;
	WSAStartup(MAKEWORD(2, 0), &wsadata);
#endif /* _WIN32 */

	/* Get initial list of open file descriptors */
	fds1 = lsof();

	CU_initialize_registry();
	CU_register_suites(g_suites_basic);
	CU_register_suites(g_suites_addr);
	CU_register_suites(g_suites_loop);
	CU_register_suites(g_suites_timer);
	CU_register_suites(g_suites_ctx);
#ifndef _WIN32
	CU_register_suites(g_suites_ipc);
#endif /* !_WIN32 */
	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();
	CU_cleanup_registry();

#ifdef _WIN32
	/* Cleanup winsock API */
	WSACleanup();
#endif /* _WIN32 */

	/* Get final list of open file descriptors */
	fds2 = lsof();

	/* Check for leaks */
	check_fds(fds1, fds2);

	/* Free resources */
	free(fds1);
	free(fds2);
	return 0;
}
