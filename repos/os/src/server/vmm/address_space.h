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

#ifndef _SRC__SERVER__VMM__ADDRESS_SPACE_H_
#define _SRC__SERVER__VMM__ADDRESS_SPACE_H_

#include <exception.h>
#include <util/avl_tree.h>

namespace Vmm {
	struct Address_range;
	class Address_space;
}


struct Vmm::Address_range : Genode::Avl_node<Address_range>
{
	Genode::uint64_t const start;
	Genode::uint64_t const size;

	Address_range(Genode::uint64_t start,
	              Genode::uint64_t size);

	Genode::uint64_t end() const { return start + size; }

	bool match(Address_range & other) const {
		return other.start >= start && other.end() <= end(); }

	Address_range & find(Address_range & bus_addr);

	struct Not_found : Exception
	{
		Not_found(Address_range & access)
		: Exception("Could not find ", access) {}
	};

	void print(Genode::Output & out) const
	{
		Genode::print(out, "address=", Genode::Hex(start),
		              " width=", Genode::Hex(size));
	}

	/************************
	 ** Avl_node interface **
	 ************************/

	bool higher(Address_range * range) {
		return range->start > start; }
};


class Vmm::Address_space
{
	private:

		Genode::Avl_tree<Address_range> _tree;

	public:

		template <typename T>
		T & get(Address_range & bus_addr)
		{
			if (!_tree.first()) throw Address_range::Not_found(bus_addr);

			return *static_cast<T*>(&_tree.first()->find(bus_addr));
		}

		void add(Address_range & ar) { _tree.insert(&ar); }
};


#endif /* _SRC__SERVER__VMM__ADDRESS_SPACE_H_ */
