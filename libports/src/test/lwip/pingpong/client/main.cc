/*
 * \brief  Ping-client
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
#include <util/string.h>
#include <os/config.h>

#include <lwip/genode.h>

/* libc includes */
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>

#include "../pingpong.h"

unsigned int verbose;

int
dial(const char *addr)
{
	int s;
	struct sockaddr_in in_addr;

	PLOG("Create new socket...");
	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == -1) {
		PERR("Could not create socket!");
		return -1;
	}

	PLOG("Connect to server %s:%d...", addr, Sport);
	in_addr.sin_port = htons(Sport);
	in_addr.sin_family = AF_INET;
	in_addr.sin_addr.s_addr = inet_addr(addr);
	if (connect(s, (struct sockaddr *)&in_addr, sizeof (in_addr)) == -1) {
		PERR("Could not connect to server!");
		close(s);
		return -1;
	}

	PLOG("Sucessful connected to server.");

	return s;
}

int
sendping(const char *addr, size_t dsize)
{
	Packet p;
	int s;
	size_t i;
	ssize_t n;

	s = dial(addr);
	if (s == -1)
		return -1;

	p.h.type = Tping;
	p.h.dsize = dsize;
	p.d = (char *)malloc(p.h.dsize);
	if (p.d == NULL) {
		PERR("Out of memory!");
		return -1;
	}

	PINF("Try to send %d packets...", Numpackets);
	for (i = 0; i < Numpackets; i++) {
		forgepacket(&p, i + 1);

		n = sendpacket(s, &p);
		if (n <= 0)
			break;
		if (n != (sizeof (Packetheader) + p.h.dsize)) {
			PERR("size mismatch: %ld != %lu", n, sizeof (Packetheader) + p.h.dsize);
			break;
		}

		if (verbose)
			PINF("%lu	%ld", p.h.id, n);
	}

	close(s);
	free(p.d);

	switch (n) {
	case 0:
		PERR("Disconnect, sent packets: %lu", i);
		return 0;
		break;
	case -1:
		PERR("Error, sent packets: %lu", i);
		return 1;
		break;
	default:
		PINF("Sucessful, sent packets: %lu", i);
		return 0;
		break;
	}

	/* never reached */
	return 0;
}

int
main(int argc, char *argv[])
{
	char serverip[16];
	unsigned int i;
	unsigned int startsize, endsize;

	/* DHCP */
	if (lwip_nic_init(0, 0, 0)) {
		PERR("We got no IP address!");
		return 1;
	}

	/* default settings */
	startsize = 1;
	endsize   = 32768;
	verbose   = 0;

	Genode::Xml_node argv_node = Genode::config()->xml_node().sub_node("argv");
	try {
		argv_node.attribute("serverip" ).value(serverip, sizeof(serverip));
		argv_node.attribute("startsize").value( &startsize );
		argv_node.attribute("endsize").value( &endsize );
		argv_node.attribute("verbose").value( &verbose );
	} catch(...) { }

	if ((endsize + sizeof (Packetheader)) > Databuf) {
		PERR("endsize is greater than the servers' data buffer");
		return 1;
	}

	for (i = startsize; i <= endsize; i <<= 1)
		sendping(serverip, i);

	return 0;
}
