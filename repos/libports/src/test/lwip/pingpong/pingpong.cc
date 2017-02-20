/*
 * \brief  PingPong
 * \author Josef Soentgen
 * \date   2013-01-24
 *
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* libc includes */
#include <stdio.h>

#include "pingpong.h"

void
forgepacket(Packet *p, uint32_t id)
{
	p->h.id = id;
	p->d[p->h.dsize - 1] = (id % 128);
}

int
checkpacket(size_t n, Packet *p)
{
	/* check size of received packet */
	if (n != (sizeof (Packetheader) + p->h.dsize)) {
		printf("ERROR: packetsize mismatch!\n");
		return -1;
	}

	/* check packet type */
	if (p->h.type != Tping) {
		printf("ERROR: wrong packet type!\n");
		return -1;
	}

	/* check payload */
	if (p->d[p->h.dsize - 1] != (char)(p->h.id % 128)) {
		printf("ERROR: packet payload corrupt, expected: %d got: %d\n", (p->h.id % 128),
		     p->d[p->h.dsize - 1]);
		return -1;
	}

	return 0;
}

size_t
sendpacket(int s, Packet *p)
{
	char *b;
	ssize_t sent;
	size_t nd, nh, dsize;

	/* send packet header */
	b = (char *)&p->h;
	nh = 0;
	while (nh < sizeof (Packetheader)) {
		sent = send(s, b + nh, sizeof (Packetheader) - nh, 0);
		switch (sent) {
		case -1:
			printf("ERROR: send(Packetheader) == -1\n");
			return nh;
			break;
		case 0:
			printf("ERROR: send(Packetheader) == 0, connection closed\n");
			return nh;
			break;
		default:
			nh += sent;
		}
	}

	/* sent packet data */
	b = (char *)p->d;
	dsize = p->h.dsize; // save data length
	nd = 0;
	while (nd < dsize) {
		sent = send(s, b + nd, dsize - nd, 0);
		switch (sent) {
		case -1:
			printf("ERROR: send(data) == -1\n");
			return nd;
			break;
		case 0:
			printf("ERROR: send(data) == 0, connection closed\n");
			return nd;
			break;
		default:
			nd += sent;
		}
	}

	return nh + nd;
}

size_t
recvpacket(int s, Packet *p, char *dbuf, size_t ldbuf)
{
	char *b;
	ssize_t r;
	size_t nd, nh, dsize;

	/* recv packet header */
	b = (char *)&p->h;
	nh = 0;
	while (nh < sizeof (Packetheader)) {
		r = recv(s, b + nh, sizeof (Packetheader) - nh, 0);
		switch (r) {
		case -1:
			printf("ERROR: recv(Packetheader) == -1\n");
			return nh;
			break;
		case 0:
			/* disconnect */
			//printf("ERROR: recv(Packetheader) == 0, connection closed\n");
			return nh;
			break;
		default:
			nh += r;
			break;
		}
	}

	if (p->h.dsize > ldbuf) {
		printf("ERROR: packet payload is too large for dbuf!\n");
		return -1;
	}

	/* receive packet data */
	dsize = p->h.dsize;
	nd = 0;
	while (nd < dsize) {
		r = recv(s, dbuf + nd, dsize - nd, 0);
		switch (r) {
		case -1:
			printf("ERROR: recv(data) == -1\n");
			return nh + nd;
			break;
		case 0:
			/* disconnect */
			printf("ERROR: recv(data) == 0, connection closed\n");
			return nh + nd;
			break;
		default:
			nd += r;
			break;
		}
	}

	return nh + nd;
}
