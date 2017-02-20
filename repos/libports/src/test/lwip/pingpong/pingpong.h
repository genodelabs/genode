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

#ifndef _PINGPONG_H_
#define _PINGPONG_H_

/* libc includes */
#include <stdint.h>

#ifdef LWIP_NATIVE
#include <lwip/sockets.h>
#define accept(a,b,c)         lwip_accept(a,b,c)
#define bind(a,b,c)           lwip_bind(a,b,c)
#define close(s)              lwip_close(s)
#define connect(a,b,c)        lwip_connect(a,b,c)
#define listen(a,b)           lwip_listen(a,b)
#define recv(a,b,c,d)         lwip_recv(a,b,c,d)
#define select(a,b,c,d,e)     lwip_select(a,b,c,d,e)
#define send(a,b,c,d)         lwip_send(a,b,c,d)
#define socket(a,b,c)         lwip_socket(a,b,c)
#else
/* libc includes */
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#endif

enum {
	Databuf    = 1024 * 1024, // data buffer for server
	Numpackets = 1024,
	Pdata      = 16384,
	Sport      = 10000,
	Tping      = 1,
	Tpong      = 2
};

typedef struct Packetheader Packetheader;
struct Packetheader
{
	uint32_t type;  // packet type
	uint32_t id;    // packet id
	uint32_t dsize; // data size
};

typedef struct Packet Packet;
struct Packet
{
	Packetheader  h;
	char         *d;
};

void forgepacket(Packet *, uint32_t);
int checkpacket(size_t, Packet *);

size_t sendpacket(int, Packet *);
size_t recvpacket(int, Packet *, char *, size_t);

#endif /* _PINGPONG_H_ */
