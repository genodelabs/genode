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

namespace Hw {

	using namespace Genode;

	struct Descriptor;

	using Level_3_page_table = Page_table_leaf<Descriptor, SIZE_LOG2_4KB,
	                                           SIZE_LOG2_2MB>;
	using Level_2_page_table = Page_table_node<Level_3_page_table, Descriptor,
	                                           SIZE_LOG2_2MB, SIZE_LOG2_1GB>;
	using Level_1_page_table = Page_table_node<Level_2_page_table, Descriptor,
	                                           SIZE_LOG2_1GB, SIZE_LOG2_512GB>;
	struct Page_table;
}


struct Hw::Descriptor : Register<64>
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

	static Page_table_entry type(access_t const v)
	{
		if (!V::get(v)) return Page_table_entry::INVALID;
		if (Type::get(v) == Type::POINTER)
			return Page_table_entry::TABLE;
		return Page_table_entry::BLOCK;
	}

	static bool present(access_t const desc) {
		return V::get(desc); }

	static bool conflicts(access_t const old, access_t const desc) {
		return V::get(old) && old != desc; }

	static addr_t address(access_t const desc) {
		return Base::bits(Ppn::get(desc)); }

	/*
	 * Creates a page-table pointer to next table entry
	 */
	static access_t create(addr_t pa)
	{
		access_t base = Base::get((access_t)pa);
		access_t desc = 0;

		Ppn::set(desc, base);
		Type::set(desc, Type::POINTER);
		V::set(desc, 1);

		return desc;
	}

	/*
	 * Creates a page-table block entry
	 */
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

	static bool writeable(access_t const desc) {
		return present(desc) && W::get(desc); }
};


struct Hw::Page_table : Level_1_page_table
{
	using Base = Level_1_page_table;

	static constexpr size_t ALIGNM_LOG2 = SIZE_LOG2_4KB;

	using Base::Base;

	Page_table(Page_table &kernel_table) : Base()
	{
		static constexpr size_t KERNEL_START_IDX =
			(Hw::Mm::KERNEL_START & VM_MASK) >> SIZE_LOG2_1GB;

		for (unsigned i = KERNEL_START_IDX; i < MAX_ENTRIES; i++)
			_entries[i] = kernel_table._entries[i];
	}

	using Array = Hw::Page_table_array<sizeof(Level_2_page_table),
	                                   _table_count(_core_vm_size(),
	                                                SIZE_LOG2_1GB,
	                                                SIZE_LOG2_2MB)>;


	static constexpr size_t VM_MASK = (1UL<< SIZE_LOG2_512GB) - 1;

	static constexpr bool sanity_check(addr_t &addr)
	{
		/* sanity check vo bits 38 to 63 must be equal */
		auto sanity = addr >> 38;
		if (sanity != 0 && sanity != 0x3ffffff)
			return false;

		/* clear bits 39 - 63 */
		addr &= VM_MASK;
		return true;
	}

	static void table_changed();

	template <typename ALLOCATOR>
	Result insert(addr_t vo, addr_t pa, size_t size,
	              Page_flags const &flags, ALLOCATOR &alloc)
	{
		if (!sanity_check(vo))
			return Page_table_error::INVALID_RANGE;
		auto result = Base::template insert(vo, pa, size, flags, alloc);
		if (result.ok()) table_changed();
		return result;
	}

	template <typename ALLOCATOR>
	void remove(addr_t vo, size_t size, ALLOCATOR &alloc)
	{
		if (!sanity_check(vo))
			return;
		Base::template remove(vo, size, alloc);
		table_changed();
	}
};

#endif /* _SRC__LIB__HW__SPEC__RISCV__PAGE_TABLE_H_ */
