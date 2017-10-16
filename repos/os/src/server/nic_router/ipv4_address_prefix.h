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

#ifndef _IPV4_ADDRESS_PREFIX_H_
#define _IPV4_ADDRESS_PREFIX_H_

/* Genode includes */
#include <net/ipv4.h>

namespace Net { class Ipv4_address_prefix; }


struct Net::Ipv4_address_prefix
{
	Ipv4_address    address;
	Genode::uint8_t prefix;

	Ipv4_address_prefix(Ipv4_address address,
	                    Ipv4_address subnet_mask);

	Ipv4_address_prefix() : prefix(32) { }

	bool valid() const { return address.valid() || prefix == 0; }

	void print(Genode::Output &output) const;

	bool prefix_matches(Ipv4_address const &ip) const;

	Ipv4_address subnet_mask() const;

	Ipv4_address broadcast_address() const;
};


namespace Genode {

	inline size_t ascii_to(char const *s, Net::Ipv4_address_prefix &result);
}


Genode::size_t Genode::ascii_to(char const *s, Net::Ipv4_address_prefix &result)
{
	/* read the leading IPv4 address, fail if there's no address */
	Net::Ipv4_address_prefix buf;
	size_t read_len = ascii_to(s, buf.address);
	if (!read_len) {
		return 0; }

	/* check for the following slash */
	s += read_len;
	if (*s != '/') {
		return 0; }
	read_len++;
	s++;

	/* read the prefix, fail if there's no prefix */
	size_t prefix_len = ascii_to_unsigned(s, buf.prefix, 10);
	if (!prefix_len) {
		return 0; }

	/* fill result and return read length */
	result = buf;
	return read_len + prefix_len;
}

#endif /* _IPV4_ADDRESS_PREFIX_H_ */
