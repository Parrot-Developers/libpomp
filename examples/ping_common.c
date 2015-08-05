/**
 * @file ping_common.c
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

#include "ping_common.h"

/**
 */
static int parse_inet_addr(const char *buf, struct sockaddr *addr,
		uint32_t *addrlen)
{
	int res = -EINVAL;
	char *ip = NULL, *sep = NULL;
	struct addrinfo *ai = NULL;

	/* Duplicate string as we need to modify it */
	ip = strdup(buf);
	if (ip == NULL)
		goto out;

	/* Find port separator */
	sep = strrchr(ip, ':');
	if (sep == NULL)
		goto out;
	*sep = '\0';

	/* Convert address and port (WIN32 returns positive value for errors) */
	if (getaddrinfo(ip, sep + 1, NULL, &ai) != 0)
		goto out;
	if (*addrlen < ai->ai_addrlen)
		goto out;
	memcpy(addr, ai->ai_addr, ai->ai_addrlen);
	*addrlen = ai->ai_addrlen;
	res = 0;

out:
	if (ai != NULL)
		freeaddrinfo(ai);
	free(ip);
	return res;
}

/**
 */
int parse_addr(const char *buf, struct sockaddr *addr, uint32_t *addrlen)
{
	int res = -EINVAL;
	struct sockaddr_un *addr_un = NULL;

	if (buf == NULL || addr == NULL || addrlen == NULL)
		goto out;
	if (*addrlen < sizeof(struct sockaddr_storage))
		goto out;

	if (strncmp(buf, "inet:", 5) == 0) {
		/* Inet v4 address */
		res = parse_inet_addr(buf + 5, addr, addrlen);
		if (res < 0)
			goto out;
	} else if (strncmp(buf, "inet6:", 6) == 0) {
		/* Inet v6 address */
		res = parse_inet_addr(buf + 6, addr, addrlen);
		if (res < 0)
			goto out;
	} else if (strncmp(buf, "unix:", 5) == 0) {
		/* Unix address */
		addr_un = (struct sockaddr_un *)addr;
		memset(addr_un, 0, sizeof(*addr_un));
		addr_un->sun_family = AF_UNIX;
		strncpy(addr_un->sun_path, buf + 5, sizeof(addr_un->sun_path));
		if (buf[5] == '@')
			addr_un->sun_path[0] = '\0';
		*addrlen = (uint32_t)sizeof(*addr_un);
	} else {
		goto out;
	}

	/* Success */
	res = 0;

out:
	return res;
}

/**
 */
void format_addr(char *buf, uint32_t buflen, const struct sockaddr *addr,
		uint32_t addrlen)
{
	char ip[64] = "";
	char port[8] = "";
	const struct sockaddr_un *addr_un = NULL;

	if (buf == NULL || buflen == 0)
		return;

	if (addr == NULL || addrlen == 0) {
		snprintf(buf, buflen, "addr:NULL");
		return;
	}

	switch (addr->sa_family) {
	case AF_INET:
		getnameinfo(addr, addrlen, ip, sizeof(ip), port, sizeof(port),
				NI_NUMERICHOST | NI_NUMERICSERV);
		snprintf(buf, buflen, "inet:%s:%s", ip, port);
		break;
	case AF_INET6:
		getnameinfo(addr, addrlen, ip, sizeof(ip), port, sizeof(port),
				NI_NUMERICHOST | NI_NUMERICSERV);
		snprintf(buf, buflen, "inet6:%s:%s", ip, port);
		break;
	case AF_UNIX:
		addr_un = (const struct sockaddr_un *)addr;
		if (addr_un->sun_path[0] == '\0')
			snprintf(buf, buflen, "unix:@%s", addr_un->sun_path + 1);
		else
			snprintf(buf, buflen, "unix:%s", addr_un->sun_path);
		break;
	default:
		snprintf(buf, buflen, "addr:family:%d", addr->sa_family);
		return;
	}
}
