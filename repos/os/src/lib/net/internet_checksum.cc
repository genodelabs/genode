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


/**************************
 ** Unit-local utilities **
 **************************/

struct Packed_uint8
{
	Genode::uint8_t value;

} __attribute__((packed));


static uint16_t checksum_of_raw_data(Packed_uint16 const *data_ptr,
                                     size_t               data_sz,
                                     addr_t               sum)
{
	/* add up bytes in pairs */
	for (; data_sz > 1; data_sz -= sizeof(Packed_uint16)) {
		sum += data_ptr->value;
		data_ptr++;
	}
	/* add left-over byte, if any */
	if (data_sz > 0) {
		sum += ((Packed_uint8 const *)data_ptr)->value;
	}
	/* fold sum to 16-bit value */
	while (addr_t const sum_rsh = sum >> 16)
		sum = (sum & 0xffff) + sum_rsh;

	/* return one's complement */
	return (uint16_t)(~sum);
}


/***********************
 ** Internet_checksum **
 ***********************/

uint16_t Net::internet_checksum(Packed_uint16 const *data_ptr,
                                size_t               data_sz)
{
	return checksum_of_raw_data(data_ptr, data_sz, 0);
}


uint16_t Net::internet_checksum_pseudo_ip(Packed_uint16   const *data_ptr,
                                          size_t                 data_sz,
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

	/* add up data bytes */
	return checksum_of_raw_data(data_ptr, data_sz, sum);
}
