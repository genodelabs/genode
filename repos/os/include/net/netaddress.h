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

/* Genode */
#include <base/stdint.h>
#include <util/string.h>
#include <base/output.h>

namespace Net {
	template <unsigned, char, bool> class Network_address;

	template <unsigned LEN, char DELIM, bool HEX>
	static inline Genode::size_t ascii_to(char const *,
	                                      Network_address<LEN, DELIM, HEX> &);
}


/**
 * Generic form of a network address.
 */
template <unsigned LEN, char DELIM, bool HEX>
struct Net::Network_address
{
	Genode::uint8_t addr[LEN];


	/******************
	 ** Constructors **
	 ******************/

	Network_address(Genode::uint8_t value = 0) {
		Genode::memset(&addr, value, LEN); }

	Network_address(void const *src) {
		Genode::memcpy(&addr, src, LEN); }


	/*********************
	 ** Helper methods  **
	 *********************/

	void copy(void *dst) const { Genode::memcpy(dst, &addr[0], LEN); }

	void print(Genode::Output &output) const
	{
		using namespace Genode;
		for (unsigned i = 0; i < LEN; i++) {
			if (!HEX) { Genode::print(output, (unsigned) addr[i]); }
			else { Genode::print(output, Hex(addr[i], Hex::OMIT_PREFIX, Hex::PAD)); }
			if (i < LEN - 1) output.out_char(DELIM);
		}
	}


	/***************
	 ** Operators **
	 ***************/

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
Genode::size_t Net::ascii_to(char const *str, Net::Network_address<LEN, DELIM, HEX> &result)
{
	using namespace Genode;

	Net::Network_address<LEN, DELIM, HEX> result_buf;
	size_t number_id = 0;
	size_t read_len  = 0;

	while (1) {

		/* read the current number */
		size_t number_len =
			ascii_to_unsigned(str, result_buf.addr[number_id],
			                  HEX ? 16 : 10);

		/* fail if there's no number */
		if (!number_len) {
			return 0; }

		/* update read length and number index */
		read_len += number_len;
		number_id++;

		/* if we have all numbers, fill result and return read length */
		if (number_id == LEN) {
			result = result_buf;
			return read_len;
		}
		/* there are numbers left, check for the delimiter */
		str += number_len;
		if (*str != DELIM) {
			return 0; }

		/* seek to next number */
		read_len++;
		str++;
	}
}

#endif /* _NET__NETADDRESS_H_ */
