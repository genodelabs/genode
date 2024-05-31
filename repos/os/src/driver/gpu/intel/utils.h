/*
 * \brief  Helper utilities for Broadwell GPU mutliplexer
 * \author Josef Soentgen
 * \data   2017-03-15
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _UTILS_H_
#define _UTILS_H_

/* Genode includes */
#include <util/interface.h>
#include <base/ram_allocator.h>

namespace Utils {

	using size_t   = Genode::size_t;
	using uint32_t = Genode::uint32_t;
	using Ram      = Genode::Ram_dataspace_capability;

	/*
	 * Backend allocator interface
	 */
	struct Backend_alloc : Genode::Interface
	{
		virtual Ram alloc(Genode::size_t) = 0;
		virtual void free(Ram) = 0;
		virtual Genode::addr_t dma_addr(Ram) = 0;
	};

	template <unsigned int ELEMENTS> class Address_map;

	inline void clflush(volatile void *addr)
	{
		asm volatile("clflush %0" : "+m" (*(volatile char *)addr));
	}

}

template <unsigned int ELEMENTS>
class Utils::Address_map
{
	using addr_t = Genode::addr_t;

	public:

		struct Element
		{
			Ram      ds_cap {   };
			addr_t   va     { 0 };
			addr_t   pa     { 0 };
			size_t   size   { 0 };

			Element() { }

			Element(Ram ds_cap, void *pa, void *va, size_t size)
			:
				ds_cap(ds_cap),
				va((addr_t)va),
				pa((addr_t)pa),
				size(size)
			{ }

			Element(Element const &other)
			:
				ds_cap(other.ds_cap), va(other.va), pa(other.pa)
			{ }

			Element &operator = (Element const &other)
			{
				ds_cap = other.ds_cap;
				va     = other.va;
				pa     = other.pa;
				size   = other.size;
				return *this;
			}

			bool valid()      { return size > 0; }
			void invalidate() { *this = Element(); };
		};

	private:

		Element _map[ELEMENTS];

	public:

		Address_map()
		{
			Genode::memset(&_map, 0, sizeof(_map));
		}

		~Address_map()
		{
			for (uint32_t i = 0; i < ELEMENTS; i++) {
				if (!_map[i].valid()) { continue; }

				Genode::error("Address_map entry ", Genode::Hex(i), " ",
				              "still valid (", _map[i].va, "/", _map[i].pa, ")");
			}
		}

		bool add(Ram ds_cap, void *pa, void *va, size_t size)
		{
			for (uint32_t i = 0; i < ELEMENTS; i++) {
				if (_map[i].valid()) { continue; }

				_map[i] = Element(ds_cap, pa, va, size);
				return true;
			}
			return false;
		}

		template <typename FN>
		void for_each(FN const &fn)
		{
			for (unsigned i = 0; i < ELEMENTS; i++) {
				if (_map[i].valid()) fn(_map[i]);
			}
		}

		addr_t phys_addr(void *va)
		{
			addr_t virt = (addr_t)va;
			for (uint32_t i = 0; i < ELEMENTS; i++) {
				if (virt >= _map[i].va && virt < (_map[i].va + _map[i].size))
				{
					Genode::off_t offset = virt - _map[i].va;
					return _map[i].pa + offset;
				}
			}
			Genode::error("phys_addr not found for ", va);
			return 0;
		}

		addr_t virt_addr(void *pa)
		{
			addr_t phys = (addr_t)pa;
			for (uint32_t i = 0; i < ELEMENTS; i++) {
				if (phys >= _map[i].pa && phys < (_map[i].pa + _map[i].size))
				{
					Genode::off_t offset = phys - _map[i].pa;
					return _map[i].va + offset;
				}
			}
			Genode::error("virt_addr not found for ", pa);
			return 0;
		}
};

#endif /* _UTILS_H_ */
