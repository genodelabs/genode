/*
 * \brief  PingPong
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

/* Genode includes */
#include <base/printf.h>

/* libc includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>

#include "pingpong.h"

void
forgepacket(Packet *p, uint32_t id)
{
	p->h.id = id;
	p->d[p->h.dsize - 1] = (id % 128);

	//PLOG("payload for %u = %u", id, (id % 128));
}

int
checkpacket(size_t n, Packet *p)
{
	/* check size of received packet */
	if (n != (sizeof (Packetheader) + p->h.dsize)) {
		PERR("packetsize mismatch!");
		return -1;
	}

	//PLOG("header: %u, data: %u", n - p->h.dsize, n - sizeof (Packetheader));

	/* check packet type */
	if (p->h.type != Tping) {
		PERR("wrong packet type!");
		return -1;
	}

	/* check payload */
	if (p->d[p->h.dsize - 1] != (p->h.id % 128)) {
		PERR("packet payload corrupt, expected: %d got: %d", (p->h.id % 128),
		     p->d[p->h.dsize - 1]);
		return -1;
	}

	return 0;
}

ssize_t
sendpacket(int s, Packet *p)
{
	char *b;
	ssize_t sent, nd, nh;
	size_t dsize;

	/* send packet header */
	b = (char *)&p->h;
	nh = 0;
	while (nh < sizeof (Packetheader)) {
		sent = send(s, b + nh, sizeof (Packetheader) - nh, 0);
		switch (sent) {
		case -1:
			PERR("send(Packetheader) == -1");
			return nh;
			break;
		case 0:
			PERR("send(Packetheader) == 0, connection closed");
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
			PERR("send(data) == -1");
			return nd;
			break;
		case 0:
			PERR("send(data) == 0, connection closed");
			return nd;
			break;
		default:
			nd += sent;
		}
	}

	return nh + nd;
}

ssize_t
recvpacket(int s, Packet *p, char *dbuf, size_t ldbuf)
{
	char *b;
	ssize_t r, nd, nh;
	size_t dsize;

	/* recv packet header */
	b = (char *)&p->h;
	nh = 0;
	while (nh < sizeof (Packetheader)) {
		r = recv(s, b + nh, sizeof (Packetheader) - nh, 0);
		switch (r) {
		case -1:
			PERR("recv(Packetheader) == -1");
			return nh;
			break;
		case 0:
			/* disconnect */
			PERR("recv(Packetheader) == 0, connection closed");
			return nh;
			break;
		default:
			nh += r;
			break;
		}
	}

	if (p->h.dsize > ldbuf) {
		PERR("packet payload is too large for dbuf!");
		return -1;
	}

	/* receive packet data */
	dsize = p->h.dsize;
	nd = 0;
	while (nd < dsize) {
		r = recv(s, dbuf + nd, dsize - nd, 0);
		switch (r) {
		case -1:
			PERR("recv(data) == -1");
			return nh + nd;
			break;
		case 0:
			/* disconnect */
			PERR("recv(data) == 0, connection closed");
			return nh + nd;
			break;
		default:
			nd += r;
			break;
		}
	}

	return nh + nd;
}
