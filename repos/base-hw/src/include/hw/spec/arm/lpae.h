/*
 * \brief   ARM v7 long physical address space extension page table format
 * \author  Stefan Kalkowski
 * \date    2014-07-07
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__SPEC__ARM__LPAE_H_
#define _SRC__LIB__HW__SPEC__ARM__LPAE_H_

#include <cpu/page_flags.h>

#include <hw/page_table.h>

namespace Hw {
	using Genode::Page_flags;

	template <unsigned> struct Page_table_descriptor;
	template <unsigned> struct Stage1_descriptor;
	template <unsigned> struct Stage2_descriptor;

	using Level_3_stage_1_translation_table =
		Page_table_leaf<Stage1_descriptor<SIZE_LOG2_4KB>,
		                SIZE_LOG2_4KB, SIZE_LOG2_2MB>;
	using Level_2_stage_1_translation_table =
		Page_table_node<Level_3_stage_1_translation_table,
		                Stage1_descriptor<SIZE_LOG2_2MB>,
		                SIZE_LOG2_2MB, SIZE_LOG2_1GB>;
	using Level_1_stage_1_translation_table =
		Page_table_node<Level_2_stage_1_translation_table,
		                Stage1_descriptor<SIZE_LOG2_1GB>,
		                SIZE_LOG2_1GB, SIZE_LOG2_512GB>;

	using Level_3_stage_2_translation_table =
		Page_table_leaf<Stage2_descriptor<SIZE_LOG2_4KB>,
		                SIZE_LOG2_4KB, SIZE_LOG2_2MB>;
	using Level_2_stage_2_translation_table =
		Page_table_node<Level_3_stage_2_translation_table,
		                Stage2_descriptor<SIZE_LOG2_2MB>,
		                SIZE_LOG2_2MB, SIZE_LOG2_1GB>;

	struct Level_1_stage_2_translation_table;
	struct Page_table;
}


template <unsigned SIZE_LOG2>
struct Hw::Page_table_descriptor : Genode::Register<64>
{
	struct Valid : Bitfield<0, 1> { };
	struct Table : Bitfield<1, 1> { };

	struct Shareability : Bitfield<8, 2>
	{
		enum { NON_SHAREABLE   = 0,
		       OUTER_SHAREABLE = 2,
		       INNER_SHAREABLE = 3 };
	};

	struct Access_flag : Bitfield<10,1> { };

	struct Output_address : Bitfield<SIZE_LOG2, 47 - SIZE_LOG2> { };

	/**
	 * Indicates that 16 adjacent entries point to contiguous
	 * memory regions
	 */
	struct Continguous_hint : Bitfield<52,1> { };

	struct Execute_never : Bitfield<54,1> { };

	struct Next_table : Bitfield<12, 36> { };

	static Page_table_entry type(access_t const v)
	{
		if (!Valid::get(v))
			return Page_table_entry::INVALID;
		return (Table::get(v)) ? Page_table_entry::TABLE
		                       : Page_table_entry::BLOCK;
	}

	static bool present(access_t const v) {
		return Valid::get(v); }

	/**
	 * Creates next table entry
	 */
	static access_t create(addr_t pa) {
		return Next_table::masked(pa) | Table::bits(1) | Valid::bits(1); }

	static addr_t address(access_t const desc)
	{
		return (type(desc) == Page_table_entry::TABLE)
			? (addr_t)Next_table::masked(desc)
			: (addr_t)Output_address::masked(desc);
	}

	static bool conflicts(access_t const old, access_t const desc) {
		return present(old) && (old != desc); }
};


template <unsigned SIZE_LOG2>
struct Hw::Stage1_descriptor : Hw::Page_table_descriptor<SIZE_LOG2>
{
	using Base = Page_table_descriptor<SIZE_LOG2>;
	using access_t = Base::access_t;

	/**
	 * Indicates memory attribute indirection register MAIR
	 */
	struct Attribute_index : Base::template Bitfield<2, 3>
	{
		enum {
			UNCACHED = Genode::Cache::UNCACHED,
			CACHED   = Genode::Cache::CACHED,
			DEVICE
		};

		static access_t create(Page_flags const &f)
		{
			if (f.type == Genode::DEVICE)
				return Attribute_index::bits(DEVICE);

			switch (f.cacheable) {
			case Genode::CACHED: return Attribute_index::bits(CACHED);
			case Genode::WRITE_COMBINED: [[fallthrough]];
			case Genode::UNCACHED: return Attribute_index::bits(UNCACHED);
			}
			return 0;
		}
	};

	struct Non_secure : Base::template Bitfield<5, 1> { };

	struct Access_permission : Base::template Bitfield<6, 2>
	{
		enum { PRIVILEGED_RW, USER_RW, PRIVILEGED_RO, USER_RO };

		static access_t create(Page_flags const &f)
		{
			bool p = f.privileged;
			if (f.writeable) {
				if (p) return Access_permission::bits(PRIVILEGED_RW);
				else   return Access_permission::bits(USER_RW);
			} else {
				if (p) return Access_permission::bits(PRIVILEGED_RO);
				else   return Access_permission::bits(USER_RO);
			}
		}
	};

	struct Not_global : Base::template Bitfield<11,1> { };

	struct Privileged_execute_never : Base::template Bitfield<53,1> { };

	struct Execute_never : Base::template Bitfield<54,1> { };

	static access_t create(addr_t const pa) { return Base::create(pa); }

	static access_t create(Page_flags const &f, addr_t const pa)
	{
		return Access_permission::create(f)
			| Attribute_index::create(f)
			| Not_global::bits(!f.global)
			| Base::Shareability::bits(Base::Shareability::INNER_SHAREABLE)
			| Base::Output_address::masked(pa)
			| Base::Access_flag::bits(1)
			| Base::Valid::bits(1)
			| Execute_never::bits(!f.executable)
			/* last entry is marked as table */
			| Base::Table::bits(SIZE_LOG2 == SIZE_LOG2_4KB);
	}

	static bool writeable(access_t desc)
	{
		access_t ap = Access_permission::get(desc);

		if (!Base::present(desc) ||
		    (ap != Access_permission::PRIVILEGED_RW &&
		     ap != Access_permission::USER_RW))
			return false;
		return true;
	}
};


template <unsigned SIZE_LOG2>
struct Hw::Stage2_descriptor : Hw::Page_table_descriptor<SIZE_LOG2>
{
	using Base = Page_table_descriptor<SIZE_LOG2>;
	using access_t = Base::access_t;

	struct Mem_attr : Base::template Bitfield<2,4>{};
	struct Hap      : Base::template Bitfield<6,2>{};

	static access_t create(addr_t const pa) { return Base::create(pa); }

	static access_t create(Page_flags const &, addr_t const pa)
	{
		return Base::Shareability::bits(Base::Shareability::INNER_SHAREABLE)
			| Base::Output_address::masked(pa)
			| Base::Access_flag::bits(1)
			| Base::Valid::bits(1)
			| Mem_attr::bits(0xf)
			| Hap::bits(0x3)
			/* last entry is marked as table */
			| Base::Table::bits(SIZE_LOG2 == SIZE_LOG2_4KB);
	}
};


struct Hw::Level_1_stage_2_translation_table
: Page_table_node<Level_2_stage_2_translation_table,
                  Stage2_descriptor<SIZE_LOG2_1GB>,
                  SIZE_LOG2_1GB, SIZE_LOG2_256GB>
{
	static constexpr Genode::size_t ALIGNM_LOG2 = SIZE_LOG2_4KB;
};


struct Hw::Page_table : Level_1_stage_1_translation_table
{
	static constexpr Genode::size_t ALIGNM_LOG2 = SIZE_LOG2_4KB;

	using Array =
		Page_table_array<sizeof(Level_2_stage_1_translation_table),
	                     _table_count(_core_vm_size(), SIZE_LOG2_512GB,
	                                  SIZE_LOG2_1GB, SIZE_LOG2_2MB)>;

	using Level_1_stage_1_translation_table::Level_1_stage_1_translation_table;

	/**
	 * On ARM we do not need to copy top-level kernel entries
	 * because the virtual-memory kernel part is hold in a separate table
	 */
	explicit Page_table(Page_table &) : Level_1_stage_1_translation_table() { }
};

#endif /* _SRC__LIB__HW__SPEC__ARM__LPAE_H_ */
