/*
 * \brief  VMM address space utility
 * \author Stefan Kalkowski
 * \date   2019-09-13
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <address_space.h>

using Vmm::Address_range;

Address_range::Address_range(Genode::uint64_t start,
                             Genode::uint64_t size)
: start(start), size(size) { }


Address_range & Address_range::find(Address_range & bus_addr)
{
	if (match(bus_addr))
		return *this;

	Address_range * ar =
		Avl_node<Address_range>::child(bus_addr.start > start);
	if (!ar) throw Not_found(bus_addr);
	return ar->find(bus_addr);
}
