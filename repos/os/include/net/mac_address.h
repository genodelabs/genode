/*
 * \brief  Media access control (MAC) address
 * \author Martin Stein
 * \date   2016-06-22
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NET__MAC_ADDRESS_H_
#define _NET__MAC_ADDRESS_H_

/* OS includes */
#include <util/string.h>
#include <net/netaddress.h>

namespace Net {
	struct Mac_address;

	static inline Genode::size_t ascii_to(char const *, Mac_address &);
}


struct Net::Mac_address : Net::Network_address<6, ':', true>
{
	using Net::Network_address<6, ':', true>::Network_address;
	bool multicast() const { return addr[0] & 1; }
};


Genode::size_t Net::ascii_to(char const *s, Net::Mac_address &mac)
{
	using namespace Genode;

	enum {
		HEX          = true,
		MAC_CHAR_LEN = 17,   /* 12 number and 6 colons */
		MAC_SIZE     = 6,
	};

	if (Genode::strlen(s) < MAC_CHAR_LEN)
		throw -1;

	char mac_str[6];
	for (int i = 0; i < MAC_SIZE; i++) {
		int hi = i * 3;
		int lo = hi + 1;

		if (!is_digit(s[hi], HEX) || !is_digit(s[lo], HEX))
			throw -1;

		mac_str[i] = (digit(s[hi], HEX) << 4) | digit(s[lo], HEX);
	}

	Genode::memcpy(mac.addr, mac_str, MAC_SIZE);

	return MAC_CHAR_LEN;
}

#endif /* _NET__MAC_ADDRESS_H_ */
