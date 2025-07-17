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

#include <base/stdint.h>
#include <base/output.h>
#include <util/string.h>

namespace Net { class Port; }

/**
 * This class makes it clear what the port integer-value means at an interface
 */
struct Net::Port
{
	Genode::uint16_t value { 0UL };

	Port() { }

	explicit Port(Genode::uint16_t const value) : value(value) { }

	bool operator == (Port const &other) const { return value == other.value; }
	bool operator != (Port const &other) const { return value != other.value; }

	void print(Genode::Output &out) const { Genode::print(out, value); }

	Genode::size_t parse(Genode::Span const &s) { return parse_unsigned(s, value); }
};

#endif /* _NET__PORT_H_ */
