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
	using namespace Genode;

	struct Address_range;
	class  Address_space;
}


class Vmm::Address_range : private Genode::Avl_node<Address_range>
{
	private:

		uint64_t const _start;
		uint64_t const _size;

		friend class Avl_node<Address_range>;
		friend class Avl_tree<Address_range>;
		friend class Address_space;

	public:

		Address_range(uint64_t start,
		              uint64_t size);
		virtual ~Address_range() {}

		uint64_t start() const { return _start;         }
		uint64_t size()  const { return _size;          }
		uint64_t end()   const { return _start + _size; }

		bool match(Address_range & other) const {
			return other._start >= _start && other.end() <= end(); }

		Address_range & find(Address_range & bus_addr);

		struct Not_found : Exception
		{
			Not_found(Address_range & access)
			: Exception("Could not find ", access) {}
		};

		void print(Genode::Output & out) const
		{
			Genode::print(out, "address=", Hex(_start),
			              " width=", Hex(_size));
		}

		/************************
		 ** Avl_node interface **
		 ************************/

		bool higher(Address_range * range) {
			return range->_start > _start; }
};


class Vmm::Address_space
{
	private:

		Avl_tree<Address_range> _tree {};

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
