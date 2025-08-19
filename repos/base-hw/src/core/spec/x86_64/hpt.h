/*
 * \brief  x86_64 AMD nested page table definitions
 * \author Benjamin Lamowski
 * \date   2024-05-16
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__PC__VIRTUALIZATION__HPT_H_
#define _CORE__SPEC__PC__VIRTUALIZATION__HPT_H_

#include <hw/page_table.h>
#include <util/register.h>

namespace Hw {
	using namespace Genode;

	struct Hpt;

	/**
	 * IA-32e common descriptor.
	 *
	 * Table entry containing descriptor fields common to all four levels.
	 */
	struct Hpt_common_descriptor : Register<64>
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
			return present(old) && (clear_mmu_flags(old) != desc); }
	};

	struct Pml4_table_descriptor : Hpt_common_descriptor
	{
		struct Pa : Bitfield<12, 48> { }; /* physical address */

		static access_t create(Page_flags const &, addr_t const)
		{
			Genode::error("512GB block mapping is not supported!");
			return 0;
		}

		static access_t create(addr_t const pa)
		{
			/* XXX: Set memory type depending on active PAT */
			static Page_flags flags { RW, EXEC, USER, NO_GLOBAL,
				                  RAM, CACHED };
			return Hpt_common_descriptor::create(flags) | Pa::masked(pa);
		}

		static Page_table_entry type(access_t const desc)
		{
			return present(desc) ? Page_table_entry::TABLE
			                     : Page_table_entry::INVALID;
		}

		static addr_t address(access_t const desc) {
			return Pa::masked(desc); }
	};

	template <unsigned PAGE_SIZE_LOG2>
	struct Hpt_page_directory_descriptor : Hpt_common_descriptor
	{
		using Base = Hpt_common_descriptor;

		struct Ps : Base::template Bitfield<7, 1> { };  /* page size */

		/**
		 * Physical address
		 */
		struct Table_pa : Base::template Bitfield<12, 36> { };

		/**
		 * Global attribute
		 */
		struct G : Base::template Bitfield<8, 1> { };

		/**
		 * Page attribute table
		 */
		struct Pat : Base::template Bitfield<12, 1> { };

		/**
		 * Physical address
		 */
		struct Pa : Base::template Bitfield<PAGE_SIZE_LOG2,
				                     48 - PAGE_SIZE_LOG2> { };

		static Page_table_entry type(access_t const desc)
		{
			if (!present(desc)) return Page_table_entry::INVALID;
			return Ps::get(desc) ? Page_table_entry::BLOCK
			                     : Page_table_entry::TABLE;
		}

		/**
		 * Creates next table entry
		 */
		static typename Base::access_t create(addr_t const pa)
		{
			/* XXX: Set memory type depending on active PAT */
			static Page_flags flags { RW, EXEC, USER, NO_GLOBAL, RAM, CACHED };
			return Base::create(flags) | Table_pa::masked(pa);
		}

		static typename Base::access_t create(Page_flags const &flags,
		                                      addr_t const pa)
		{
			bool const wc = flags.cacheable == Cache::WRITE_COMBINED;

			return Base::create(flags)
			     | Ps::bits(1)
			     | G::bits(flags.global)
			     | Pa::masked(pa)
			     | Base::Pwt::bits(wc ? 1 : 0);
		}

		static addr_t address(access_t const desc)
		{
			return (type(desc) == Page_table_entry::TABLE)
				? Table_pa::masked(desc) : Pa::masked(desc);
		}
	};


	struct Page_table_entry_descriptor : Hpt_common_descriptor
	{
		using Common = Hpt_common_descriptor;

		struct Pat : Bitfield<7, 1> { };          /* page attribute table */
		struct G   : Bitfield<8, 1> { };          /* global               */
		struct Pa  : Bitfield<12, 36> { };        /* physical address     */

		static access_t create(Page_flags const &flags, addr_t const pa)
		{
			bool const wc = flags.cacheable == Cache::WRITE_COMBINED;

			return Common::create(flags)
				| G::bits(flags.global)
				| Pa::masked(pa)
				| Pwt::bits(wc ? 1 : 0);
		}

		static addr_t address(access_t const desc) {
			return Pa::masked(desc); }
	};


	using Level1_translation_table =
		Page_table_leaf<Page_table_entry_descriptor, SIZE_LOG2_4KB,
		                SIZE_LOG2_2MB>;
	using Pd = Page_table_node<Level1_translation_table,
	                           Hpt_page_directory_descriptor<SIZE_LOG2_2MB>,
	                           SIZE_LOG2_2MB, SIZE_LOG2_1GB>;
	using Pdpt = Page_table_node<Pd,
	                             Hpt_page_directory_descriptor<SIZE_LOG2_1GB>,
	                             SIZE_LOG2_1GB, SIZE_LOG2_512GB>;
}


struct Hw::Hpt : Page_table_node<Pdpt, Pml4_table_descriptor,
                                 SIZE_LOG2_512GB, SIZE_LOG2_256TB>
{
	using Base = Page_table_node<Pdpt, Pml4_table_descriptor,
	                             SIZE_LOG2_512GB, SIZE_LOG2_256TB>;
	using Base::Base;
	using Array =
		Page_table_array<sizeof(Pd),
	                     _table_count(_core_vm_size(), SIZE_LOG2_512GB,
	                                  SIZE_LOG2_1GB, SIZE_LOG2_2MB)>;
};

#endif /* _CORE__SPEC__PC__VIRTUALIZATION__HPT_H_ */
