/*
 * \brief  Ping-server
 * \author Josef Soentgen
 * \date   2013-01-24
 *
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* libc includes */
#include <stdio.h>

#ifdef LWIP_NATIVE
#include <nic/packet_allocator.h>
#include <lwip/genode.h>
#endif

#include "../pingpong.h"

unsigned int verbose;

int
announce(const char *addr)
{
	int s;
	struct sockaddr_in in_addr;

	printf("Create new socket...\n");
	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == -1) {
		printf("ERROR: Could not create socket!\n");
		return -1;
	}

	printf("Bind socket to %d\n", Sport);
	in_addr.sin_port = htons(Sport);
	in_addr.sin_family = AF_INET;
	in_addr.sin_addr.s_addr = inet_addr(addr);
	if ( bind(s, (struct sockaddr *)&in_addr, sizeof (in_addr)) == -1) {
		printf("ERROR: Could not bind!\n");
		close(s);
		return -1;
	}

	return s;
}

int
recvping(const char *addr)
{
	int s, c;

	struct sockaddr caddr;
	socklen_t lcaddr = sizeof (caddr);

	s = announce(addr);
	if (s == -1)
		return -1;

	printf("Listen on %s:%d...\n", addr, Sport);
	if (listen(s, 5) == -1) {
		printf("ERROR: Could not listen!\n");
		close(s);
		return -1;
	}

	while (1) {
		Packet p;
		int act;
		size_t packets;
		ssize_t packet_size = 0;
		ssize_t n;

		printf("wait...\n");
		c = accept(s, &caddr, &lcaddr);
		if (c == -1) {
			printf("ERROR: Invalid socket from accept()!\n");
			continue;
		}
		printf("client %d connected...\n", c);

		p.d = (char *)malloc(Databuf);
		if (p.d == NULL) {
			printf("ERROR: Out of memeory!\n");
			close(c);
			break;
		}

		/* receive packets from client */
		act = 1; packets = 0;
		while (act) {

			fd_set rfds;
			FD_ZERO(&rfds);
			FD_SET(c, &rfds);

			if (select(c + 1, &rfds, NULL, NULL, NULL) == -1)
				printf("ERROR: select() == -1\n");

			n = recvpacket(c, &p, p.d, Databuf);
			switch (n) {
			case 0:
				/* disconnect */
				//printf("ERROR: disconnect\n");
				close(c);
				act = 0;
				break;
			default:
				/* check if packet is valid */
				if (checkpacket(n, &p)) {
					act = 0;
				} else {
					packets++;
					packet_size = n;
				}
				break;
			}

			if (verbose)
				printf("%u	%zd\n", p.h.id, n);
		}
		printf("received %zu packets of size %zu\n", packets, packet_size);

		free(p.d);
	}

	close(s);

	return 0;
}

int
main(int argc, char *argv[])
{
	char listenip[16] = "0.0.0.0";

#ifdef LWIP_NATIVE
	enum { BUF_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE * 128 };

	lwip_tcpip_init();
	/* DHCP */
	if (lwip_nic_init(0, 0, 0, BUF_SIZE, BUF_SIZE)) {
		printf("ERROR: We got no IP address!\n");
		return 1;
	}
#endif

	verbose = 0;

	recvping(listenip);

	return 0;
}
