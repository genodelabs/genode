/*
 * \brief  Ping-client
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

#include "../pingpong.h"

unsigned int verbose;

int
dial(const char *addr)
{
	int s;
	struct sockaddr_in in_addr;

	printf("Create new socket...\n");
	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == -1) {
		printf("ERROR: Could not create socket!\n");
		return -1;
	}

	printf("Connect to server %s:%d...\n", addr, Sport);
	in_addr.sin_port = htons(Sport);
	in_addr.sin_family = AF_INET;
	in_addr.sin_addr.s_addr = inet_addr(addr);
	if (connect(s, (struct sockaddr *)&in_addr, sizeof (in_addr)) == -1) {
		printf("ERROR: Could not connect to server!\n");
		close(s);
		return -1;
	}

	printf("Sucessfully connected to server.\n");

	return s;
}

int
sendping(const char *addr, size_t dsize, size_t count)
{
	Packet p;
	int s;
	size_t i;
	size_t n = 0;

	s = dial(addr);
	if (s == -1)
		return -1;

	p.h.type = Tping;
	p.h.dsize = dsize;
	p.d = (char *)malloc(p.h.dsize);
	if (p.d == NULL) {
		printf("ERROR: Out of memory!\n");
		return -1;
	}

	printf("Trying to send %zd packets...\n", count);
	for (i = 0; i < count; i++) {
		forgepacket(&p, i + 1);

		n = sendpacket(s, &p);
		if (n == 0)
			break;
		if (n != (sizeof (Packetheader) + p.h.dsize)) {
			printf("ERROR: size mismatch: %ld != %lu\n",
			       (long)n, (long)(sizeof (Packetheader) + p.h.dsize));
			break;
		}

		if (verbose)
			printf("%u	%ld\n", (unsigned)p.h.id, (long)n);
	}

	close(s);
	free(p.d);

	switch (n) {
	case 0:
		printf("Disconnect, sent packets: %zu\n", i);
		return 0;
		break;
	default:
		printf("Sucessfull, sent packets: %zu\n", i);
		return 0;
		break;
	}

	/* never reached */
	return 0;
}

int
main(int argc, char *argv[])
{
	char serverip[16] = "0.0.0.0";
	unsigned int i;
	unsigned int startsize, endsize;
	unsigned int count;

	/* default settings */
	startsize = 1;
	endsize   = 32768;
	count     = 1024;
	verbose   = 0;

#if 0
	Genode::Xml_node argv_node = Genode::config()->xml_node().sub_node("argv");
	try {
		argv_node.attribute("serverip" ).value(serverip, sizeof(serverip));
		argv_node.attribute("startsize").value( &startsize );
		argv_node.attribute("endsize").value( &endsize );
		argv_node.attribute("verbose").value( &verbose );
	} catch(...) { }
#endif

	for (int i = 1; i < argc; i += 2) {
		if (strcmp(argv[i], "-serverip") == 0)
			strncpy(serverip, argv[i+1], sizeof(serverip));
		else if (strcmp(argv[i], "-startsize") == 0)
			startsize = atoi(argv[i+1]);
		else if (strcmp(argv[i], "-endsize") == 0)
			endsize = atoi(argv[i+1]);
		else if (strcmp(argv[i], "-count") == 0)
			count = atoi(argv[i+1]);
	}

	if ((endsize + sizeof (Packetheader)) > Databuf) {
		printf("ERROR: endsize is greater than the servers' data buffer\n");
		return 1;
	}

	for (i = startsize; i <= endsize; i <<= 1)
		sendping(serverip, i, count);

	return 0;
}
