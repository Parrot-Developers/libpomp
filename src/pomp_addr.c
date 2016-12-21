/**
 * @file pomp_addr.c
 *
 * @brief Socket address parsing/formatting utilities.
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

#include "pomp_priv.h"

/**
 * Parse a inet socket address given as a string and convert it to sockaddr.
 * @param buf: input string.
 * @param addr: destination structure.
 * @param addrlen: maximum size of destination structure as input, real size
 * converted as output.
 * @return 0 in case of success, negative errno value in case of error.
 */
static int pomp_addr_parse_inet(const char *buf, struct sockaddr *addr,
		uint32_t *addrlen)
{
	int res = -EINVAL;
	struct addrinfo hints;
	char *ip = NULL, *sep = NULL;
	struct addrinfo *ai = NULL;

	/* Setup hints for getaddrinfo */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = 0;
	hints.ai_flags = AI_NUMERICHOST;
	hints.ai_protocol = 0;

	/* Duplicate string as we need to modify it */
	ip = strdup(buf);
	if (ip == NULL)
		goto out;

	/* Find port separator */
	sep = strrchr(ip, ':');
	if (sep == NULL)
		goto out;
	*sep = '\0';

	/* Convert address and port
	 * WIN32 returns positive value for errors */
	res = getaddrinfo(ip, sep + 1, &hints, &ai);
	if (res != 0) {
		POMP_LOGE("getaddrinfo(%s:%s): err=%d(%s)", ip, sep + 1,
				res, gai_strerror(res));
		res = -EINVAL;
		goto out;
	}
	if (*addrlen < (uint32_t)ai->ai_addrlen) {
		res = -EINVAL;
		goto out;
	}
	memcpy(addr, ai->ai_addr, ai->ai_addrlen);
	*addrlen = ai->ai_addrlen;
	res = 0;

out:
	/* Free resources */
	if (ai != NULL)
		freeaddrinfo(ai);
	free(ip);
	return res;
}

/*
 * See documentation in public header.
 */
int pomp_addr_parse(const char *buf, struct sockaddr *addr, uint32_t *addrlen)
{
	int res = -EINVAL;
	struct sockaddr_un *addr_un = NULL;

	POMP_RETURN_ERR_IF_FAILED(buf != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(addr != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(addrlen != NULL, -EINVAL);

	if (strncmp(buf, "inet:", 5) == 0) {
		/* Inet v4 address */
		res = pomp_addr_parse_inet(buf + 5, addr, addrlen);
		if (res < 0)
			goto out;
	} else if (strncmp(buf, "inet6:", 6) == 0) {
		/* Inet v6 address */
		res = pomp_addr_parse_inet(buf + 6, addr, addrlen);
		if (res < 0)
			goto out;
	} else if (strncmp(buf, "unix:", 5) == 0) {
		/* Unix address */
		if (*addrlen < sizeof(struct sockaddr_un))
			goto out;
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

/*
 * See documentation in public header.
 */
int pomp_addr_format(char *buf, uint32_t buflen, const struct sockaddr *addr,
		uint32_t addrlen)
{
	char ip[64] = "";
	char port[8] = "";
	const struct sockaddr_un *addr_un = NULL;

	POMP_RETURN_ERR_IF_FAILED(buf != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(buflen != 0, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(addr != NULL, -EINVAL);
	POMP_RETURN_ERR_IF_FAILED(addrlen >= sizeof(struct sockaddr), -EINVAL);

	/* WIN32 returns positive value for errors for getnameinfo */
	switch (addr->sa_family) {
	case AF_INET:
		if (getnameinfo(addr, addrlen, ip, sizeof(ip),
				port, sizeof(port),
				NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
			return -EINVAL;
		}
		snprintf(buf, buflen, "inet:%s:%s", ip, port);
		break;

	case AF_INET6:
		if (getnameinfo(addr, addrlen, ip, sizeof(ip),
				port, sizeof(port),
				NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
			return -EINVAL;
		}
		snprintf(buf, buflen, "inet6:%s:%s", ip, port);
		break;

	case AF_UNIX:
		POMP_RETURN_ERR_IF_FAILED(addrlen >= sizeof(struct sockaddr_un),
				-EINVAL);
		addr_un = (const struct sockaddr_un *)addr;
		if (addr_un->sun_path[0] == '\0')
			snprintf(buf, buflen, "unix:@%s", addr_un->sun_path+1);
		else
			snprintf(buf, buflen, "unix:%s", addr_un->sun_path);
		break;

	default:
		snprintf(buf, buflen, "addr:family:%d", addr->sa_family);
		break;
	}

	return 0;
}

/*
 * See documentation in public header.
 */
int pomp_addr_is_unix(const struct sockaddr *addr, uint32_t addrlen)
{
	if (addr == NULL || addrlen < sizeof(struct sockaddr))
		return 0;
	return addr->sa_family == AF_UNIX;
}
