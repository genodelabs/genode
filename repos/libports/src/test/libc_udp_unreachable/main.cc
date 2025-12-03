/*
 * \brief  libc UDP unreachable test
 * \author Sebastian Sumpf
 * \date   2025-12-03
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

static char const *server = "10.0.1.2";
static int const  port    = 80;

#define DIE() \
	{ printf("Error: '%s' failed - %s:%d\n", __func__, __FILE__, __LINE__); exit(-1); }


static void test_udp_unreachable()
{
	int fd = socket(AF_INET, SOCK_DGRAM, 0);

	struct sockaddr_in addr { };
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(server);

	char buf[64] = { };

	sockaddr const *paddr = reinterpret_cast<sockaddr const *>(&addr);

	ssize_t ret = sendto(fd, buf, sizeof(buf), 0, paddr, sizeof(addr));

	if (ret != -1 || errno != ENETUNREACH) DIE();
}


int main(int argc, char *argv[])
{
	test_udp_unreachable();

	return 0;
}
