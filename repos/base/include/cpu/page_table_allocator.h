/*
 * \brief  Page table allocator
 * \author Stefan Kalkowski
 * \author Johannes Schlatow
 * \date   2015-06-10
 */

/*
 * Copyright (C) 2015-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__CPU__PAGE_TABLE_ALLOCATOR_H_
#define _INCLUDE__CPU__PAGE_TABLE_ALLOCATOR_H_

#include <util/bit_allocator.h>
#include <util/construct_at.h>

namespace Genode {
	template <Genode::size_t TABLE_SIZE> class Page_table_allocator;
	struct Out_of_tables {};
}

template <Genode::size_t TABLE_SIZE>
class Genode::Page_table_allocator
{
	protected:

		using addr_t = Genode::addr_t;
		using size_t = Genode::size_t;

		addr_t const  _virt_addr;
		addr_t const  _phys_addr;
		size_t const  _size;

		template <typename TABLE> addr_t _offset(TABLE & table) {
			return (addr_t)&table - _virt_addr; }

		void * _index(unsigned idx) {
			return (void*)(_virt_addr + TABLE_SIZE*idx); }

		virtual unsigned _alloc()            = 0;
		virtual void     _free(unsigned idx) = 0;

	public:

		template <unsigned COUNT> class Array;

		Page_table_allocator(addr_t virt_addr, addr_t phys_addr, size_t size)
		: _virt_addr(virt_addr), _phys_addr(phys_addr), _size(size) { }

		virtual ~Page_table_allocator() { }

		template <typename TABLE, typename FN1, typename FN2>
		void with_table(addr_t phys_addr, FN1 && match_fn, FN2 no_match_fn)
		{
			static_assert((sizeof(TABLE) == TABLE_SIZE), "unexpected size");

			if (phys_addr >= _phys_addr && phys_addr < _phys_addr + _size)
				match_fn(*(TABLE*)(_virt_addr + (phys_addr - _phys_addr)));
			else
				no_match_fn();
		}

		template <typename TABLE> addr_t construct()
		{
			static_assert((sizeof(TABLE) == TABLE_SIZE), "unexpected size");
			TABLE & table = *Genode::construct_at<TABLE>(_index(_alloc()));
			return _offset(table) + _phys_addr;
		}

		template <typename TABLE> void destruct(addr_t phys_addr)
		{
			static_assert((sizeof(TABLE) == TABLE_SIZE), "unexpected size");

			with_table<TABLE>(phys_addr,
				[&] (TABLE & table) {
					table.~TABLE();
					_free((unsigned)(_offset(table) / sizeof(TABLE)));
				},
				[&] () {
				Genode::error("Trying to destruct foreign table at ", Genode::Hex(phys_addr));
				});
		}

		size_t size() const { return _size; }
};


template <Genode::size_t TABLE_SIZE>
template <unsigned COUNT>
class Genode::Page_table_allocator<TABLE_SIZE>::Array
{
	public:

		class Allocator;

	private:

		struct Table { Genode::uint8_t data[TABLE_SIZE]; };

		Table     _tables[COUNT];
		Allocator _alloc;

	public:

		Array() : _alloc((Table*)&_tables, (addr_t)&_tables, COUNT * TABLE_SIZE) {}

		template <typename T>
		explicit Array(T phys_addr)
		: _alloc(_tables, phys_addr((void*)_tables), COUNT * TABLE_SIZE) { }

		Page_table_allocator<TABLE_SIZE> & alloc() { return _alloc; }
};


template <Genode::size_t TABLE_SIZE>
template <unsigned COUNT>
class Genode::Page_table_allocator<TABLE_SIZE>::Array<COUNT>::Allocator
:
	public Genode::Page_table_allocator<TABLE_SIZE>
{
	private:

		using Bit_allocator = Genode::Bit_allocator<COUNT>;
		using Array = Page_table_allocator<TABLE_SIZE>::Array<COUNT>;

		Bit_allocator _free_tables { };

		unsigned _alloc() override
		{
			try {
				return (unsigned)_free_tables.alloc();
			} catch (typename Bit_allocator::Out_of_indices&) {}
			throw Out_of_tables();
		}

		void _free(unsigned idx) override { _free_tables.free(idx); }

	public:

		Allocator(Table * tables, addr_t phys_addr, size_t size)
		: Page_table_allocator((addr_t)tables, phys_addr, size) {}

		Allocator(addr_t phys_addr, addr_t virt_addr, size_t size)
		: Page_table_allocator(virt_addr, phys_addr, size),
		  _free_tables(static_cast<Allocator*>(&reinterpret_cast<Array*>(virt_addr)->alloc())->_free_tables)
		{
			static_assert(!__is_polymorphic(Bit_allocator),
			              "base class needs to be non-virtual");
		}
};

#endif /* _INCLUDE__CPU__PAGE_TABLE_ALLOCATOR_H_ */
