/*
 * \brief  Generic network address definitions
 * \author Stefan Kalkowski
 * \date   2010-08-20
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NET__NETADDRESS_H_
#define _NET__NETADDRESS_H_

#include <base/stdint.h>
#include <util/string.h>

namespace Net {
	template <unsigned, char, bool> class Network_address;
}


/**
 * Generic form of a network address.
 */
template <unsigned LEN, char DELIM, bool HEX>
struct Net::Network_address
{
	Genode::uint8_t addr[LEN];

	Network_address(Genode::uint8_t value = 0) {
		Genode::memset(&addr, value, LEN); }

	Network_address(void const *src) {
		Genode::memcpy(&addr, src, LEN); }

	void copy(void *dst) const { Genode::memcpy(dst, &addr[0], LEN); }

	void print(Genode::Output &output) const
	{
		using Genode::print;
		using Genode::Hex;

		for (unsigned i = 0; i < LEN; i++) {
			if (HEX)
				print(output, Hex(addr[i], Hex::OMIT_PREFIX, Hex::PAD));
			else
				print(output, unsigned(addr[i]));

			if (i < LEN - 1)
				output.out_char(DELIM);
		}
	}

	inline Genode::size_t parse(Genode::Span const &s);

	bool operator==(const Network_address &other) const {

		/*
		 * We compare from lowest address segment to highest
		 * one, because in a local context, the higher segments
		 * of two addresses normally don't distinguish.
		 * (e.g. in an IPv4 local subnet)
		 */
		for (int i = LEN-1; i >= 0; --i) {
			if (addr[i] != other.addr[i])
				return false;
		}
		return true;
	}

	bool operator!=(const Network_address &other) const {
		return !(*this == other); }
}
__attribute__((packed));


template <unsigned LEN, char DELIM, bool HEX>
inline Genode::size_t Net::Network_address<LEN, DELIM, HEX>::parse(Genode::Span const &s)
{
	using namespace Genode;

	char const *str = s.start;

	Genode::uint8_t result[LEN];

	size_t i = 0;
	size_t read_len  = 0;

	while (1) {
		if (!s.contains(str))
			return 0;

		/* read the current number */
		size_t number_len =
			parse_unsigned(Span { str, s.num_bytes - read_len }, result[i], HEX ? 16 : 10);

		/* fail if there's no number */
		if (!number_len)
			return 0;

		/* update read length and number index */
		read_len += number_len;
		i++;

		/* if we have all numbers, update object and return read length */
		if (i == LEN) {
			Genode::memcpy(addr, result, LEN);
			return read_len;
		}

		/* there are numbers left, check for the delimiter */
		str += number_len;
		if (!s.contains(str) || *str != DELIM)
			return 0;

		/* seek to next number */
		read_len++;
		str++;
	}
}

#endif /* _NET__NETADDRESS_H_ */
