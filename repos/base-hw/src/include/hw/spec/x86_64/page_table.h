/*
 * \brief  x86_64 page table definitions
 * \author Stefan Kalkowski
 * \author Adrian-Ken Rueegsegger
 * \date   2015-02-06
 */

/*
 * Copyright (C) 2015-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__SPEC__X86_64__PAGE_TABLE_H_
#define _SRC__LIB__HW__SPEC__X86_64__PAGE_TABLE_H_

#include <base/log.h>
#include <cpu/page_flags.h>

#include <hw/memory_consts.h>
#include <hw/page_table.h>

namespace Hw {
	struct Descriptor;
	struct Leaf_descriptor;
	template <Genode::size_t> struct Directory_descriptor;
	struct Top_level_descriptor;

	/**
	 * IA-32e page table (Level 4)
	 *
	 * A page table consists of 512 entries that each maps a 4KB page
	 * frame. For further details refer to Intel SDM Vol. 3A, table 4-19.
	 */
	using Page_table_4 =
		Page_table_leaf<Leaf_descriptor, SIZE_LOG2_4KB, SIZE_LOG2_2MB>;

	using Page_table_3 =
		Page_table_node<Page_table_4, Directory_descriptor<SIZE_LOG2_2MB>,
		                SIZE_LOG2_2MB, SIZE_LOG2_1GB>;

	using Page_table_2 =
		Page_table_node<Page_table_3, Directory_descriptor<SIZE_LOG2_1GB>,
		                SIZE_LOG2_1GB, SIZE_LOG2_512GB>;
	struct Page_table;
}


/**
 * IA-32e common descriptor.
 *
 * Table entry containing descriptor fields common to all four levels.
 */
struct Hw::Descriptor : Genode::Register<64>
{
	struct P   : Bitfield<0, 1> { };   /* present         */
	struct Rw  : Bitfield<1, 1> { };   /* read/write      */
	struct Us  : Bitfield<2, 1> { };   /* user/supervisor */
	struct Pwt : Bitfield<3, 1> { };   /* write-through or PAT defined */
	struct Pcd : Bitfield<4, 1> { };   /* cache disable or PAT defined */
	struct A   : Bitfield<5, 1> { };   /* accessed        */
	struct D   : Bitfield<6, 1> { };   /* dirty           */
	struct Xd  : Bitfield<63, 1> { };  /* execute-disable */

	static bool present(access_t const v) { return P::get(v); }

	static access_t create(Page_flags const &flags)
	{
		return P::bits(1)
			| Rw::bits(flags.writeable)
			| Us::bits(!flags.privileged)
			| Xd::bits(!flags.executable);
	}

	/**
	 * Return descriptor value with cleared accessed and dirty flags. These
	 * flags can be set by the MMU.
	 */
	static access_t clear_mmu_flags(access_t const value)
	{
		access_t ret = value;
		A::clear(ret);
		D::clear(ret);
		return ret;
	}

	static bool conflicts(access_t const old, access_t const desc) {
		return (present(old) && clear_mmu_flags(old) != desc); }

	static bool writeable(access_t const desc) {
		return present(desc) && Rw::get(desc); }
};


struct Hw::Leaf_descriptor : Hw::Descriptor
{
	struct Pat : Bitfield<7, 1> { };          /* page attribute table */
	struct G   : Bitfield<8, 1> { };          /* global               */
	struct Pa  : Bitfield<12, 36> { };        /* physical address     */
	struct Mt  : Genode::Bitset_3<Pwt, Pcd, Pat> { }; /* memory type  */

	static access_t create(Page_flags const &flags, addr_t const pa)
	{
		bool const wc = flags.cacheable == Cache::WRITE_COMBINED;

		return Descriptor::create(flags)
			| G::bits(flags.global)
			| Pa::masked(pa)
			| Pwt::bits(wc ? 1 : 0);
	}

	static addr_t address(access_t const desc) {
		return Pa::masked(desc); }
};


template <Genode::size_t SIZE_LOG2>
struct Hw::Directory_descriptor : Hw::Descriptor
{
	struct Ps : Bitfield<7, 1> { };  /* page size */

	/**
	 * Global attribute
	 */
	struct G : Bitfield<8, 1> { };

	/**
	 * Page attribute table
	 */
	struct Pat : Bitfield<12, 1> { };

	/**
	 * Physical address
	 */
	struct Pa : Bitfield<SIZE_LOG2, 48 - SIZE_LOG2> { };

	/**
	 * Memory type
	 */
	struct Mt : Genode::Bitset_3<Pwt, Pcd, Pat> { };

	/**
	 * Physical address of next table
	 */
	struct Table_pa : Bitfield<12, 36> { };

	/**
	 * Memory types of next table
	 */
	struct Table_mt : Genode::Bitset_2<Pwt, Pcd> { };

	static Page_table_entry type(access_t const desc)
	{
		if (!present(desc)) return Page_table_entry::INVALID;
		return (Ps::get(desc)) ? Page_table_entry::BLOCK
		                       : Page_table_entry::TABLE;
	}

	static access_t create(Page_flags const &flags, addr_t const pa)
	{
		bool const wc =
			flags.cacheable == Genode::Cache::WRITE_COMBINED;

		return Descriptor::create(flags)
		     | Ps::bits(1)
		     | G::bits(flags.global)
		     | Pa::masked(pa)
		     | Pwt::bits(wc ? 1 : 0);
	}

	static access_t create(addr_t const pa)
	{
		/* XXX: Set memory type depending on active PAT */
		static Page_flags flags { RW, EXEC, USER, NO_GLOBAL,
		                          RAM, Genode::CACHED };
		return Descriptor::create(flags) | Table_pa::masked(pa);
	}

	static addr_t address(access_t const desc)
	{
		switch (type(desc)) {
		case Page_table_entry::INVALID: break;
		case Page_table_entry::TABLE: return Table_pa::masked(desc);
		case Page_table_entry::BLOCK: return Pa::masked(desc);
		};
		return 0;
	}
};


struct Hw::Top_level_descriptor : Hw::Descriptor
{
	struct Pa : Bitfield<12, SIZE_LOG2_256TB> { }; /* physical address */
	struct Mt : Genode::Bitset_2<Descriptor::Pwt,
	                             Descriptor::Pcd> { }; /* memory type */

	static Descriptor::access_t create(Page_flags const &, addr_t const)
	{
		/* assumimg that full 512GB aligned dataspaces are never attached */
		return 0;
	};

	static access_t create(addr_t const pa)
	{
		/* XXX: Set memory type depending on active PAT */
		static Page_flags flags { RW, EXEC, USER, NO_GLOBAL,
		                          RAM, Genode::CACHED };
		return Descriptor::create(flags) | Pa::masked(pa);
	}

	static addr_t address(access_t const desc) {
		return Pa::masked(desc); }

	static Page_table_entry type(access_t const desc)
	{
		return (!present(desc)) ? Page_table_entry::INVALID
	                            : Page_table_entry::TABLE;
	}
};


struct Hw::Page_table
: Hw::Page_table_node<Hw::Page_table_2, Hw::Top_level_descriptor,
                      SIZE_LOG2_512GB, SIZE_LOG2_256TB>
{
	using Base = Page_table_node<Page_table_2, Top_level_descriptor,
	                             SIZE_LOG2_512GB, SIZE_LOG2_256TB>;

	using Base::Base;

	static constexpr size_t ALIGNM_LOG2 = SIZE_LOG2_4KB;

	explicit Page_table(Page_table &kernel_table) : Base()
	{
		static constexpr size_t SIZE_MASK = (1UL << SIZE_LOG2_256TB) - 1;
		static constexpr size_t KERNEL_START_IDX =
			(Hw::Mm::KERNEL_START & SIZE_MASK) >> SIZE_LOG2_512GB;

		/* copy kernel page directories to new table */
		for (size_t i = KERNEL_START_IDX; i < Base::MAX_ENTRIES; i++)
			_entries[i] = kernel_table._entries[i];
	}

	using Array =
		Page_table_array<sizeof(Page_table_2),
	                     _table_count(_core_vm_size(), SIZE_LOG2_512GB,
	                                  SIZE_LOG2_1GB, SIZE_LOG2_2MB)>;
};

#endif /* _SRC__LIB__HW__SPEC__X86_64__PAGE_TABLE_H_ */
