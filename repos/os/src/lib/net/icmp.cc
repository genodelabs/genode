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
#include <net/internet_checksum.h>
#include <net/icmp.h>
#include <base/output.h>

using namespace Net;
using namespace Genode;


void Net::Icmp_packet::print(Output &output) const
{
	Genode::print(output, "ICMP ", (unsigned)type(), " ", (unsigned)code());
}


void Icmp_packet::update_checksum(size_t data_sz)
{
	_checksum = 0;
	_checksum = internet_checksum((Packed_uint16 *)this, sizeof(Icmp_packet) + data_sz);
}


bool Icmp_packet::checksum_error(size_t data_sz) const
{
	return internet_checksum((Packed_uint16 *)this, sizeof(Icmp_packet) + data_sz);
}
