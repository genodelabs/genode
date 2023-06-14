/*
 * \brief  Transmission Control Protocol
 * \author Martin Stein
 * \date   2016-06-15
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <net/internet_checksum.h>
#include <net/tcp.h>
#include <base/output.h>

using namespace Net;
using namespace Genode;


void Net::Tcp_packet::print(Genode::Output &output) const
{
	Genode::print(output, "TCP ", src_port(), " > ", dst_port(), " flags '");

	if (fin()) { Genode::print(output, "f"); }
	if (syn()) { Genode::print(output, "s"); }
	if (rst()) { Genode::print(output, "r"); }
	if (psh()) { Genode::print(output, "p"); }
	if (ack()) { Genode::print(output, "a"); }
	if (urg()) { Genode::print(output, "u"); }
	if (ece()) { Genode::print(output, "e"); }
	if (cwr()) { Genode::print(output, "c"); }
	if (ns())  { Genode::print(output, "n"); }
	Genode::print(output, "' ");
}


void Net::Tcp_packet::update_checksum(Ipv4_address ip_src,
                                      Ipv4_address ip_dst,
                                      size_t       tcp_size)
{
	_checksum = 0;
	_checksum = internet_checksum_pseudo_ip((Packed_uint16 *)this, tcp_size,
	                                        host_to_big_endian((uint16_t)tcp_size),
	                                        Ipv4_packet::Protocol::TCP, ip_src, ip_dst);
}
