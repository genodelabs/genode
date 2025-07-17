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

#include <net/ipv4.h>
#include <util/string.h>

namespace Net { class Ipv4_address_prefix; }


struct Net::Ipv4_address_prefix
{
	Ipv4_address    address { };
	Genode::uint8_t prefix;

	Ipv4_address_prefix(Ipv4_address address,
	                    Ipv4_address subnet_mask);

	Ipv4_address_prefix() : prefix(32) { }

	bool valid() const { return address.valid() || prefix == 0; }

	void print(Genode::Output &output) const;

	bool prefix_matches(Ipv4_address const &ip) const;

	Ipv4_address subnet_mask() const;

	Ipv4_address broadcast_address() const;

	bool operator != (Ipv4_address_prefix const &other) const
	{
		return prefix  != other.prefix ||
		       address != other.address;
	}

	inline Genode::size_t parse(Genode::Span const&);
};


inline Genode::size_t Net::Ipv4_address_prefix::parse(Genode::Span const &span)
{
	using namespace Genode;

	/* read the leading IPv4 address, fail if there's no address */
	Net::Ipv4_address_prefix buf;
	size_t read_len = buf.address.parse(span);
	if (!read_len)
		return 0;

	/* check for the following slash incl. at least 1 digit */
	char const *s = span.start + read_len;
	if (!span.contains(s + 1) || *s != '/')
		return 0;
	read_len++;
	s++;

	/* read the prefix, fail if there's no prefix */
	size_t prefix_len = parse_unsigned(Span { s, span.num_bytes - read_len }, buf.prefix, 10);
	if (!prefix_len)
		return 0;

	/* fill result and return read length */
	address = buf.address;
	prefix  = buf.prefix;
	return read_len + prefix_len;
}

#endif /* _IPV4_ADDRESS_PREFIX_H_ */
