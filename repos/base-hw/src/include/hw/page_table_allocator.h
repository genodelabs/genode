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

#include <base/memory.h>
#include <util/bit_allocator.h>
#include <util/construct_at.h>

namespace Hw {
	using namespace Genode;

	enum class Page_table_error {
		OUT_OF_RAM, OUT_OF_CAPS, DENIED, INVALID_RANGE };

	class Page_table_allocator;

	template <size_t TABLE_SIZE, unsigned COUNT>
	class Page_table_array;
}


class Hw::Page_table_allocator : protected Memory::Constrained_allocator
{
	public:

		using Error  = Page_table_error;
		using Result = Attempt<Ok, Error>;

	protected:

		struct Lookup_error {};

		virtual Attempt<addr_t, Lookup_error> _phys_addr(addr_t virt_addr) = 0;
		virtual Attempt<addr_t, Lookup_error> _virt_addr(addr_t phys_addr) = 0;

	public:

		template <typename TABLE>
		Result lookup(addr_t phys_addr, auto const fn)
		{
			return _virt_addr(phys_addr).convert<Result>(
				[&] (addr_t virt_addr) -> Result {
					return fn(*((TABLE*)virt_addr)); },
				[&] (Lookup_error) -> Result { return Error::INVALID_RANGE; });
		}

		template <typename TABLE, typename ENTRY>
		Result create(typename ENTRY::access_t &descriptor)
		{
			return try_alloc(sizeof(TABLE)).convert<Result>(
				[&] (Constrained_allocator::Allocation &bytes) -> Result {

					construct_at<TABLE>(bytes.ptr);

					/* hand over ownership to caller of 'create' */
					bytes.deallocate = false;
					return _phys_addr((addr_t)bytes.ptr).convert<Result>(
						[&] (addr_t phys_addr) {
							descriptor = ENTRY::create(phys_addr);
							return Ok();
						},
						[&] (Lookup_error) -> Result { return Error::DENIED; });
				},
				[&] (Alloc_error e) -> Result {
					switch (e) {
					case Alloc_error::OUT_OF_CAPS: return Error::OUT_OF_CAPS;
					case Alloc_error::OUT_OF_RAM:  return Error::OUT_OF_RAM;
					case Alloc_error::DENIED:      break;
					};
					return Error::DENIED;
				 });
		}

		template <typename TABLE>
		void destroy(TABLE &table)
		{
			table.~TABLE();

			/* deallocate bytes via ~Allocation */
			Constrained_allocator::Allocation { *this, { &table, sizeof(table) } };
		}
};


template <Genode::size_t TABLE_SIZE, unsigned COUNT>
class Hw::Page_table_array
{
	public:

		class Allocator;

	private:

		struct Table { Genode::uint8_t data[TABLE_SIZE]; };

		Table _tables[COUNT];

		Allocator _alloc;

	public:

		Page_table_array()
		: _alloc((Table*)&_tables, (addr_t)&_tables) {}

		template <typename T>
		explicit Page_table_array(T phys_addr)
		: _alloc(_tables, phys_addr((void*)_tables)) { }

		Page_table_array(Page_table_array &a, addr_t phys_addr)
		: _alloc(phys_addr, a) { }

		Page_table_allocator& alloc() { return _alloc; }
};


template <Genode::size_t TABLE_SIZE, unsigned COUNT>
class Hw::Page_table_array<TABLE_SIZE, COUNT>::Allocator
:
	public Hw::Page_table_allocator
{
	private:

		using Bit_allocator = Genode::Bit_allocator<COUNT>;
		using Array = Page_table_array<TABLE_SIZE, COUNT>;
		using addr_t = Genode::addr_t;

		static constexpr size_t SIZE = TABLE_SIZE * COUNT;
		addr_t const _virt_base;
		addr_t const _phys_base;

		Bit_allocator _free_tables { };

		Attempt<addr_t, Lookup_error> _phys_addr(addr_t virt_addr) override
		{
			if (virt_addr < _virt_base || virt_addr > (_virt_base+SIZE))
				return Lookup_error();
			return (virt_addr - _virt_base) + _phys_base;
		}

		Attempt<addr_t, Lookup_error> _virt_addr(addr_t phys_addr) override
		{
			if (phys_addr < _phys_base || phys_addr > (_phys_base+SIZE))
				return Lookup_error();
			return (phys_addr - _phys_base) + _virt_base;
		}

	public:

		Allocator(Table * tables, addr_t phys_addr)
		: _virt_base((addr_t)tables), _phys_base(phys_addr) {}

		Allocator(addr_t phys_addr, Page_table_array &a)
		:
			_virt_base((addr_t)&a),
			_phys_base(phys_addr),
		  	_free_tables(static_cast<Allocator&>(a.alloc())._free_tables)
		{
			static_assert(!__is_polymorphic(Bit_allocator),
			              "base class needs to be non-virtual");
		}

		Allocation::Attempt try_alloc(size_t num_bytes) override
		{
			using Result = Allocation::Attempt;

			if (num_bytes != TABLE_SIZE)
				warning("ignore given allocation size of ", num_bytes);

			return _free_tables.alloc().template convert<Result>(
				[&] (addr_t idx) -> Result {
					return { *this,
					         { (void*)(idx*TABLE_SIZE+_virt_base), TABLE_SIZE }};
				},
				[&] (Bit_allocator::Error) -> Result {
					return Genode::Alloc_error::DENIED; });
		}

		void _free(Allocation &a) override
		{
			addr_t addr = (addr_t)a.ptr;
			if (addr < _virt_base)
				return;

			unsigned idx = (unsigned)((addr -_virt_base) / TABLE_SIZE);
			_free_tables.free(idx);
		}
};

#endif /* _SRC__LIB__HW__PAGE_TABLE_ALLOCATOR_H_ */
