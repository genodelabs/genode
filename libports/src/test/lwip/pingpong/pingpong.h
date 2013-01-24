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

#ifndef _PINGPONG_H_
#define _PINGPONG_H_

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

ssize_t sendpacket(int, Packet *);
ssize_t recvpacket(int, Packet *, char *, size_t);

#endif /* _PINGPONG_H_ */
