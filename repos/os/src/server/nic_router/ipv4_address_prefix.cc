/*
 * \brief  Ipv4 address combined with a subnet prefix length
 * \author Martin Stein
 * \date   2017-10-12
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <ipv4_address_prefix.h>

using namespace Genode;
using namespace Net;


Ipv4_address Ipv4_address_prefix::subnet_mask() const
{
	Ipv4_address result;
	if (prefix >= 8) {

		result.addr[0] = 0xff;

		if (prefix >= 16) {

			result.addr[1] = 0xff;

			if (prefix >= 24) {

				result.addr[2] = 0xff;
				result.addr[3] = 0xff << (32 - prefix);
			} else {
				result.addr[2] = 0xff << (24 - prefix);
			}
		} else {
			result.addr[1] = 0xff << (16 - prefix);
		}
	} else {
		result.addr[0] = 0xff << (8 - prefix);
	}
	return result;
}


void Ipv4_address_prefix::print(Genode::Output &output) const
{
	Genode::print(output, address, "/", prefix);
}


bool Ipv4_address_prefix::prefix_matches(Ipv4_address const &ip) const
{
	uint8_t prefix_left = prefix;
	uint8_t byte = 0;
	for (; prefix_left >= 8; prefix_left -= 8, byte++) {
		if (ip.addr[byte] != address.addr[byte]) {
			return false; }
	}
	if (prefix_left == 0) {
		return true; }

	uint8_t const mask = ~(0xff >> prefix_left);
	return !((ip.addr[byte] ^ address.addr[byte]) & mask);
}


Ipv4_address Ipv4_address_prefix::broadcast_address() const
{
	Ipv4_address result = address;
	Ipv4_address const mask = subnet_mask();
	for (unsigned i = 0; i < 4; i++) {
		result.addr[i] |= ~mask.addr[i];
	}
	return result;
}


Ipv4_address_prefix::Ipv4_address_prefix(Ipv4_address address,
                                         Ipv4_address subnet_mask)
:
	address(address), prefix(0)
{
	Genode::uint8_t rest;
	if        (subnet_mask.addr[0] != 0xff) {
		rest = subnet_mask.addr[0];
		prefix = 0;
	} else if (subnet_mask.addr[1] != 0xff) {
		rest = subnet_mask.addr[1];
		prefix = 8;
	} else if (subnet_mask.addr[2] != 0xff) {
		rest = subnet_mask.addr[2];
		prefix = 16;
	} else {
		rest = subnet_mask.addr[3];
		prefix = 24;
	}
	for (Genode::uint8_t mask = 1 << 7; rest & mask; mask >>= 1)
		prefix++;
}
