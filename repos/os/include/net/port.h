/*
 * \brief  Network port
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NET__PORT_H_
#define _NET__PORT_H_

/* Genode includes */
#include <base/stdint.h>
#include <base/output.h>
#include <util/string.h>

namespace Net { class Port; }

/**
 * This class makes it clear what the port integer-value means at an interface
 */
struct Net::Port
{
	Genode::uint16_t value;

	explicit Port(Genode::uint16_t const value) : value(value) { }

	bool operator == (Port const &other) const { return value == other.value; }

	void print(Genode::Output &out) const { Genode::print(out, value); }
}
__attribute__((packed));


namespace Genode {

	/**
	 * Read port value from string
	 *
	 * \return number of consumed characters
	 */
	inline size_t ascii_to(const char *s, Net::Port &result)
	{
		unsigned value = 0;
		size_t const consumed = ascii_to_unsigned(s, value, 0);
		result = Net::Port(value);
		return consumed;
	}
}

#endif /* _NET__PORT_H_ */
