/*
 * \brief  Page table allocator
 * \author Stefan Kalkowski
 * \date   2015-06-10
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__PAGE_TABLE_ALLOCATOR_H_
#define _SRC__LIB__HW__PAGE_TABLE_ALLOCATOR_H_

#include <util/bit_allocator.h>
#include <util/construct_at.h>

namespace Hw {
	template <Genode::size_t TABLE_SIZE> class Page_table_allocator;
	struct Out_of_tables {};
}

template <Genode::size_t TABLE_SIZE>
class Hw::Page_table_allocator
{
	protected:

		using addr_t = Genode::addr_t;

		addr_t const  _virt_addr;
		addr_t const  _phys_addr;

		template <typename TABLE> addr_t _offset(TABLE & table) {
			return (addr_t)&table - _virt_addr; }

		void * _index(unsigned idx) {
			return (void*)(_virt_addr + TABLE_SIZE*idx); }

		virtual unsigned _alloc()            = 0;
		virtual void     _free(unsigned idx) = 0;

	public:

		template <unsigned COUNT> class Array;

		Page_table_allocator(addr_t virt_addr, addr_t phys_addr)
		: _virt_addr(virt_addr), _phys_addr(phys_addr) { }

		virtual ~Page_table_allocator() { }

		template <typename TABLE> addr_t phys_addr(TABLE & table) {
			static_assert((sizeof(TABLE) == TABLE_SIZE), "unexpected size");
			return _offset(table) + _phys_addr; }

		template <typename TABLE> TABLE & virt_addr(addr_t phys_addr) {
			static_assert((sizeof(TABLE) == TABLE_SIZE), "unexpected size");
			return *(TABLE*)(_virt_addr + (phys_addr - _phys_addr)); }

		template <typename TABLE> TABLE & construct() {
			static_assert((sizeof(TABLE) == TABLE_SIZE), "unexpected size");
			return *Genode::construct_at<TABLE>(_index(_alloc())); }

		template <typename TABLE> void destruct(TABLE & table)
		{
			static_assert((sizeof(TABLE) == TABLE_SIZE), "unexpected size");
			table.~TABLE();
			_free(_offset(table) / sizeof(TABLE));
		}
};


template <Genode::size_t TABLE_SIZE>
template <unsigned COUNT>
class Hw::Page_table_allocator<TABLE_SIZE>::Array
{
	public:

		class Allocator;

	private:

		struct Table { Genode::uint8_t data[TABLE_SIZE]; };

		Table     _tables[COUNT];
		Allocator _alloc;

	public:

		Array() : _alloc((Table*)&_tables, (addr_t)&_tables) {}

		template <typename T>
		explicit Array(T phys_addr)
		: _alloc(_tables, phys_addr((void*)_tables)) { }

		Page_table_allocator<TABLE_SIZE> & alloc() { return _alloc; }
};


template <Genode::size_t TABLE_SIZE>
template <unsigned COUNT>
class Hw::Page_table_allocator<TABLE_SIZE>::Array<COUNT>::Allocator
: public Hw::Page_table_allocator<TABLE_SIZE>
{
	private:

		using Bit_allocator = Genode::Bit_allocator<COUNT>;
		using Array = Page_table_allocator<TABLE_SIZE>::Array<COUNT>;

		Bit_allocator _free_tables { };

		unsigned _alloc() override
		{
			try {
				return _free_tables.alloc();
			} catch (typename Bit_allocator::Out_of_indices&) {}
			throw Out_of_tables();
		}

		void _free(unsigned idx) override { _free_tables.free(idx); }

	public:

		Allocator(Table * tables, addr_t phys_addr)
		: Page_table_allocator((addr_t)tables, phys_addr) {}

		Allocator(addr_t phys_addr, addr_t virt_addr)
		: Page_table_allocator(virt_addr, phys_addr),
		  _free_tables(static_cast<Allocator*>(&reinterpret_cast<Array*>(virt_addr)->alloc())->_free_tables)
		{
			static_assert(!__is_polymorphic(Bit_allocator),
			              "base class needs to be non-virtual");
		}
};

#endif /* _SRC__LIB__HW__PAGE_TABLE_ALLOCATOR_H_ */
