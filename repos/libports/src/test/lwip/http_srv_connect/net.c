/*
 * \brief  Simple http_srv client
 * \author Josef Soentgen
 * \date   2012-8-30
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
dial(struct addrinfo *ai)
{
	struct addrinfo *aip;
	int s, err;

	for (aip = ai; aip != NULL; aip->ai_next) {
		s = socket(aip->ai_family, aip->ai_socktype,
		           aip->ai_protocol);
		if (s == -1) {
			perror("socket");
			continue;
		}

		err = connect(s, aip->ai_addr, aip->ai_addrlen);
		if (err != -1)
			break;

		close(s);
		s = -1;
	}

	return s;
}


struct addrinfo *
lookup(const char *proto, const char *host, const char *port)
{
	struct addrinfo hints, *r;
	int err, socktype, protocol;

	if (proto == NULL) {
		fprintf(stderr, "protocol is not set\n");
		return NULL;
	}

	/**
	 * actually we can simply use protcol == 0 but we
	 * set it explicitly anyway.
	 */
	if (strcmp(proto, "tcp") == 0) {
		socktype = SOCK_STREAM;
		protocol = IPPROTO_TCP;
	}
	else if (strcmp(proto, "udp") == 0) {
		socktype = SOCK_DGRAM;
		protocol = IPPROTO_UDP;
	}
	else {
		fprintf(stderr, "protocol '%s' invalid\n", proto);
		return NULL;
	}

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = socktype;
	hints.ai_flags = 0;
	hints.ai_protocol = protocol;

	err = getaddrinfo(host, port, &hints, &r);
	if (err != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
		return NULL;
	}

	return r;
}
