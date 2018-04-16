/*
 * \brief  Computing the Internet Checksum (conforms to RFC 1071)
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
#include <net/internet_checksum.h>

using namespace Net;
using namespace Genode;


uint16_t Net::internet_checksum(uint16_t const *addr,
                                size_t          size,
                                addr_t          init_sum)
{
	/* add up bytes in pairs */
	addr_t sum = init_sum;
	for (; size > 1; size -= 2)
		sum += *addr++;

	/* add left-over byte, if any */
	if (size > 0)
		sum += *(uint8_t *)addr;

	/* fold sum to 16-bit value */
	while (addr_t const sum_rsh = sum >> 16)
		sum = (sum & 0xffff) + sum_rsh;

	/* return one's complement */
	return ~sum;
}


uint16_t Net::internet_checksum_pseudo_ip(uint16_t        const *ip_data,
                                          size_t                 ip_data_sz,
                                          uint16_t               ip_data_sz_be,
                                          Ipv4_packet::Protocol  ip_prot,
                                          Ipv4_address          &ip_src,
                                          Ipv4_address          &ip_dst)
{
	/*
	 * Add up pseudo IP header:
	 *
	 *  --------------------------------------------------------------
	 * | src-ipaddr | dst-ipaddr | zero-field | prot.-id |  data size |
	 * |  4 bytes   |  4 bytes   |   1 byte   |  1 byte  |  2 bytes   |
	 *  --------------------------------------------------------------
	 */
	addr_t sum = host_to_big_endian((uint16_t)ip_prot) + ip_data_sz_be;
	for (size_t i = 0; i < Ipv4_packet::ADDR_LEN; i += 2)
		sum += *(uint16_t*)&ip_src.addr[i] + *(uint16_t*)&ip_dst.addr[i];

	/* add up IP data bytes */
	return internet_checksum(ip_data, ip_data_sz, sum);
}
