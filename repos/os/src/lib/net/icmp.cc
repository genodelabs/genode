/*
 * \brief  Internet Control Message Protocol
 * \author Martin Stein
 * \date   2018-03-23
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <net/icmp.h>
#include <base/output.h>

using namespace Net;
using namespace Genode;


void Net::Icmp_packet::print(Output &output) const
{
	Genode::print(output, "\033[32mICMP\033[0m ", (unsigned)type(), " ",
	                      (unsigned)code());
}


uint16_t Icmp_packet::calc_checksum(size_t data_sz) const
{
	/* do not sum-up checksum itself */
	register long sum   = _type + _code;
	addr_t        addr  = (addr_t)&_rest_of_header_u32;
	size_t        count = sizeof(Icmp_packet) + data_sz - sizeof(_type) -
	                      sizeof(_code) - sizeof(_checksum);

	/* sum-up rest of header and data */
	while (count > 1) {
		sum   += *(uint16_t *)addr;
		addr  += sizeof(uint16_t);
		count -= sizeof(uint16_t);
	}
	/* add left-over byte, if any */
	if (count) {
		sum += *(uint8_t *)addr;
	}
	/*  fold 32-bit sum to 16 bits */
	while (sum >> 16) {
		sum = (sum & 0xffff) + (sum >> 16);
	}
	/* write to header */
	uint16_t sum16 = ~sum;
	return host_to_big_endian(sum16);
}
