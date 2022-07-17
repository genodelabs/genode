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
	Genode::print(output, "\033[32mICMP\033[0m ", (unsigned)type(), " ",
	                      (unsigned)code());
}


void Icmp_packet::update_checksum(size_t data_sz)
{
	_checksum = 0;
	_checksum = internet_checksum((Packed_uint16 *)this, sizeof(Icmp_packet) + data_sz);
}


void Icmp_packet::update_checksum(Internet_checksum_diff const &icd)
{
	_checksum = icd.apply_to(_checksum);
}


bool Icmp_packet::checksum_error(size_t data_sz) const
{
	return internet_checksum((Packed_uint16 *)this, sizeof(Icmp_packet) + data_sz);
}


void Icmp_packet::query_id(uint16_t v, Internet_checksum_diff &icd)
{
	uint16_t const v_be { host_to_big_endian(v) };
	icd.add_up_diff((
		Packed_uint16 *)&v_be, (Packed_uint16 *)&_rest_of_header_u16[0], 2);

	_rest_of_header_u16[0] = v_be;
}


void Icmp_packet::type_and_code(Type t, Code c, Internet_checksum_diff &icd)
{
	uint16_t const old_type_and_code { *(uint16_t*)this };
	type(t);
	code(c);
	icd.add_up_diff(
		(Packed_uint16 *)&_type, (Packed_uint16 *)&old_type_and_code, 2);
}
