/*
 * \brief  RISCV Sv39 page table format
 * \author Sebastian Sumpf
 * \date   2015-08-04
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__SPEC__RISCV__PAGE_TABLE_H_
#define _SRC__LIB__HW__SPEC__RISCV__PAGE_TABLE_H_

#include <base/log.h>
#include <cpu/page_flags.h>

#include <hw/memory_consts.h>
#include <hw/page_table.h>

namespace Sv39 {

	using namespace Genode;
	using namespace Hw;

	template <typename ENTRY, unsigned BLOCK_SIZE_LOG2, unsigned SIZE_LOG2>
	struct Level_x_page_table;

	struct Level_3_page_table;

	using Level_2_page_table =
		Level_x_page_table<Level_3_page_table, SIZE_LOG2_2MB, SIZE_LOG2_1GB>;

	using Level_1_page_table =
		Level_x_page_table<Level_2_page_table, SIZE_LOG2_1GB, SIZE_LOG2_512GB>;

	struct Descriptor;
	struct Table_descriptor;
	struct Block_descriptor;
}


struct Sv39::Descriptor : Register<64>
{
	enum Descriptor_type { INVALID, TABLE, BLOCK };
	struct V : Bitfield<0, 1> { }; /* present */
	struct R : Bitfield<1, 1> { }; /* read */
	struct W : Bitfield<2, 1> { }; /* write */
	struct X : Bitfield<3, 1> { }; /* executable */
	struct U : Bitfield<4, 1> { }; /* user */
	struct G : Bitfield<5, 1> { }; /* global  */
	struct A : Bitfield<6, 1> { }; /* access bit */
	struct D : Bitfield<7, 1> { }; /* dirty bit */

	struct Perm : Bitfield<0, 5> { };
	struct Type : Bitfield<1, 3>
	{
		enum {
			POINTER = 0
		};
	};
	struct Ppn  : Bitfield<10, 38> { }; /* physical address 10 bit aligned */
	struct Base : Bitfield<12, 38> { }; /* physical address page aligned */

	static access_t permission_bits(Genode::Page_flags const &f)
	{
		access_t rights = 0;
		R::set(rights, 1);

		if (f.writeable)
			W::set(rights, 1);

		if (f.executable)
			X::set(rights, 1);

		if (!f.privileged)
			U::set(rights, 1);

		if (f.global)
			G::set(rights, 1);

		return rights;
	}

	static Descriptor_type type(access_t const v)
	{
		if (!V::get(v)) return INVALID;
		if (Type::get(v) == Type::POINTER)
			return TABLE;

		return BLOCK;
	}

	static bool present(access_t const v) {
		return V::get(v); }
};


struct Sv39::Table_descriptor : Descriptor
{
	static access_t create(addr_t pa)
	{
		access_t base = Base::get((access_t)pa);
		access_t desc = 0;

		Ppn::set(desc, base);
		Type::set(desc, Type::POINTER);
		V::set(desc, 1);

		return desc;
	}
};


struct Sv39::Block_descriptor : Descriptor
{
	static access_t create(Genode::Page_flags const &f, addr_t const pa)
	{
		access_t base = Base::get(pa);
		access_t desc = 0;

		Ppn::set(desc, base);
		Perm::set(desc, permission_bits(f));

		/*
		 * Always set access and dirty bits because RISC-V may raise a page fault
		 * (implementation dependend) in case it observes this bits being cleared.
		 */
		A::set(desc, 1);
		if (f.writeable)
			D::set(desc, 1);

		V::set(desc, 1);

		return desc;
	}
};


template <typename ENTRY, unsigned BLOCK_SIZE_LOG2, unsigned SIZE_LOG2>
struct Sv39::Level_x_page_table
: Hw::Page_table_tpl<Descriptor, BLOCK_SIZE_LOG2, SIZE_LOG2>
{
	static constexpr size_t VM_MASK = (1UL<< SIZE_LOG2_512GB) - 1;

	using Base = Page_table_tpl<Descriptor, BLOCK_SIZE_LOG2, SIZE_LOG2>;
	using Result = Base::Result;

	void table_changed();

	static constexpr bool sanity_check(addr_t &addr)
	{
		/* sanity check vo bits 38 to 63 must be equal */
		addr_t sanity = addr >> 38;
		if (sanity != 0 && sanity != 0x3ffffff)
			return false;

		/* clear bits 39 - 63 */
		addr &= VM_MASK;
		return true;
	}

	using desc_t = typename Descriptor::access_t;

	/**
	 * Insert translations into this table
	 *
	 * \param vo     offset of the virtual region represented
	 *               by the translation within the virtual
	 *               region represented by this table
	 * \param pa     base of the physical backing store
	 * \param size   size of the translated region
	 * \param flags  mapping flags
	 * \param alloc  level allocator
	 */
	Result insert(addr_t vo, addr_t pa, size_t size,
	              Page_flags const &flags, Page_table_allocator &alloc)
	{
		if (!sanity_check(vo))
			return Page_table_error::INVALID_RANGE;

		Result result = Base::_for_range(vo, pa, size,
			[&] (addr_t vo, addr_t pa, size_t size, desc_t &desc) -> Result
			{
				using Td = Table_descriptor;

				/* can we insert a whole block? */
				if (Base::_aligned_and_fits(vo, pa, size)) {
					desc_t blk_desc = Block_descriptor::create(flags, pa);

					if (Descriptor::present(desc) && desc != blk_desc)
						return Page_table_error::INVALID_RANGE;

					desc = blk_desc;
					return Ok();
				}

				/* we need to use a next level table */
				switch (Descriptor::type(desc)) {

				case Descriptor::INVALID: /* no entry */
					{
						/* create and link next level table */
						Result r = alloc.create<ENTRY, Td>(desc);
						if (r.failed())
							return r;
					}
					[[fallthrough]];

				case Descriptor::TABLE: /* table already available */
					{
						addr_t phys = Td::Base::bits(Td::Ppn::get(desc));
						return alloc.lookup<ENTRY>(phys, [&] (ENTRY &e) {
							return e.insert(vo-Base::_page_mask_high(vo),
						 	                    pa, size, flags, alloc); });
					}

				case Descriptor::BLOCK: /* there is already a block */
					break;
				};

				return Page_table_error::INVALID_RANGE;
			});

		table_changed();
		return result;
	}

	/**
	 * Remove translations that overlap with a given virtual region
	 *
	 * \param vo    region offset within the tables virtual region
	 * \param size  region size
	 * \param alloc level allocator
	 */
	void remove(addr_t vo, size_t size, Page_table_allocator &alloc)
	{
		if (!sanity_check(vo))
			return;

		Base::_for_range(vo, size,
			[&] (addr_t vo, size_t size, desc_t &desc) {
			using Td = Table_descriptor;

			switch (Descriptor::type(desc)) {
			case Descriptor::TABLE:
				/* use allocator to retrieve virt address of table */
				alloc.lookup<ENTRY>(Td::Base::bits(Td::Ppn::get(desc)),
					[&] (ENTRY &table) {
					table.remove(vo-Base::_page_mask_high(vo), size, alloc);
					if (table.empty()) {
						alloc.destroy<ENTRY>(table);
						desc = 0;
					}
					return Ok();
				}).with_error([] (Page_table_error) { /* ignore */ });
				return;
			case Descriptor::BLOCK: [[fallthrough]];
			case Descriptor::INVALID:
				desc = 0;
			}
		});

		table_changed();
	}

	Result lookup(addr_t, addr_t&, Page_table_allocator&)
	{
		Genode::raw(__func__, " not implemented yet");
		return Page_table_error::INVALID_RANGE;
	}
};


struct Sv39::Level_3_page_table
: Page_table_tpl<Descriptor, SIZE_LOG2_4KB, SIZE_LOG2_2MB>
{
	using Base = Page_table_tpl<Descriptor, SIZE_LOG2_4KB, SIZE_LOG2_2MB>;
	using Result = Base::Result;
	using desc_t = typename Descriptor::access_t;

	Result insert(addr_t vo, addr_t pa, size_t size,
	              Page_flags const &flags, Page_table_allocator&)
	{
		return Base::_for_range(vo, pa, size,
			[&] (addr_t vo, addr_t pa, size_t size, desc_t &desc) -> Result
			{
				if (!_aligned_and_fits(vo, pa, size))
					return Page_table_error::INVALID_RANGE;

				desc_t blk_desc = Block_descriptor::create(flags, pa);

				if (Descriptor::present(desc) && desc != blk_desc)
					return Page_table_error::INVALID_RANGE;

				desc = blk_desc;
				return Ok();
			});
	}

	void remove(addr_t vo, size_t size, Page_table_allocator &)
	{
		Base::_for_range(vo, size,
		                 [&] (addr_t, size_t, desc_t &desc) { desc = 0; });
	}
};


namespace Hw { struct Page_table; }


struct Hw::Page_table : Sv39::Level_1_page_table
{
	static constexpr size_t ALIGNM_LOG2 = SIZE_LOG2_4KB;

	using Sv39::Level_1_page_table::Level_1_page_table;

	Page_table(Page_table &kernel_table)
	:
		Sv39::Level_1_page_table()
	{
		static constexpr size_t KERNEL_START_IDX =
			(Hw::Mm::KERNEL_START & VM_MASK) >> SIZE_LOG2_1GB;

		for (unsigned i = KERNEL_START_IDX; i < MAX_ENTRIES; i++)
			_entries[i] = kernel_table._entries[i];
	}

	using Array = Hw::Page_table_array<sizeof(Sv39::Level_2_page_table),
	                                   _table_count(_core_vm_size(),
	                                                SIZE_LOG2_1GB,
	                                                SIZE_LOG2_2MB)>;
} __attribute__((aligned(1 << ALIGNM_LOG2)));

#endif /* _SRC__LIB__HW__SPEC__RISCV__PAGE_TABLE_H_ */
