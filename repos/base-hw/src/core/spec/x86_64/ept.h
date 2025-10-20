/*
 * \brief  EPT page table definitions
 * \author Benjamin Lamowski
 * \date   2024-04-23
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__PC__VIRTUALIZATION__EPT_H_
#define _CORE__SPEC__PC__VIRTUALIZATION__EPT_H_

#include <cpu/page_flags.h>
#include <hw/page_table.h>
#include <util/register.h>

namespace Hw {
	using namespace Genode;

	struct Ept;

	/*
	 * Common EPT Permissions
	 *
	 * For further details see Intel SDM Vol. 3C
	 * Table 29-2. Format of an EPT PML4 Entry (PML4E) that References an
	 * EPT Page-Directory-Pointer Table
	 */
	struct Ept_common_descriptor : Genode::Register<64>
	{
		struct R   : Bitfield< 0,1> { }; /* Read */
		struct W   : Bitfield< 1,1> { }; /* Write */
		struct X   : Bitfield< 2,1> { }; /* Execute */
		struct A   : Bitfield< 8,1> { }; /* Accessed */
		struct D   : Bitfield< 9,1> { }; /* Dirty (ignored in tables) */
		struct UX  : Bitfield<10,1> { }; /* User-mode execute access */

		static bool present(access_t const v) { return R::get(v); }

		static access_t create(Page_flags const &flags)
		{
			return R::bits(1)
				| W::bits(flags.writeable)
				| UX::bits(!flags.privileged)
				| X::bits(flags.executable);
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


	struct Pml4e_table_descriptor : Ept_common_descriptor
	{
		struct Pa : Bitfield<12, 48> { }; /* Physical address */

		static access_t create(Page_flags const &, addr_t const)
		{
			Genode::error("512GB block mapping is not supported!");
			return 0;
		}

		static access_t create(addr_t const pa)
		{
			/* XXX: Set memory type depending on active PAT */
			static Page_flags flags { RW, EXEC, USER, NO_GLOBAL,
				                  RAM, Genode::CACHED };
			return Ept_common_descriptor::create(flags) | Pa::masked(pa);
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
	struct Ept_page_directory_descriptor : Ept_common_descriptor
	{
		using Base = Ept_common_descriptor;

		struct Ps : Base::template Bitfield<7, 1> { };  /* Page size */

		static Page_table_entry type(access_t const desc)
		{
			if (!present(desc)) return Page_table_entry::INVALID;
			return Ps::get(desc) ? Page_table_entry::BLOCK
			                     : Page_table_entry::TABLE;
		}

		/**
		 * Physical address
		 */
		struct Table_pa : Base::template Bitfield<12, 36> { };

		static typename Base::access_t create(addr_t const pa)
		{
			static Page_flags flags { RW, EXEC, USER, NO_GLOBAL,
			                          RAM, Genode::CACHED };
			return Base::create(flags) | Table_pa::masked(pa);
		}

		struct Type : Bitfield< 3,3> { }; /* Ept memory type, see Section 29.3.7 */
		struct Pat  : Bitfield< 6,1> { }; /* Ignore PAT memory type, see 29.3.7 */

		/**
		 * Physical address
		 */
		struct Pa : Base::template Bitfield<PAGE_SIZE_LOG2,
		                                     48 - PAGE_SIZE_LOG2> { };

		static typename Base::access_t create(Page_flags const &flags,
		                                      addr_t const pa)
		{
			return Base::create(flags)
			     | Ps::bits(1)
			     | Pa::masked(pa)
			     | Type::bits(6)
			     | Pat::bits(1);
		}

		static addr_t address(access_t const desc)
		{
			return (type(desc) == Page_table_entry::TABLE)
				? Table_pa::masked(desc) : Pa::masked(desc);
		}
	};


	/* Table 29-7. Format of an EPT Page-Table Entry that Maps a 4-KByte Page */
	struct Ept_page_table_entry_descriptor : Ept_common_descriptor
	{
		using Common = Ept_common_descriptor;

		struct Type : Bitfield< 3, 3> { }; /* Ept memory type, see Section 29.3.7 */
		struct Pat  : Bitfield< 6, 1> { }; /* Ignore PAT memory type, see 29.3.7 */
		struct Pa   : Bitfield<12,36> { }; /* Physical address */

		static access_t create(Page_flags const &flags, addr_t const pa)
		{
			return Common::create(flags) | Pa::masked(pa) | Type::bits(6) | Pat::bits(1);
		}

		static addr_t address(access_t const desc) {
			return Pa::masked(desc); }
	};

	using Ept_page_table = Page_table_leaf<Ept_page_table_entry_descriptor,
	                                       SIZE_LOG2_4KB, SIZE_LOG2_2MB>;
	using Ept_pd = Page_table_node<Ept_page_table,
	                               Ept_page_directory_descriptor<SIZE_LOG2_2MB>,
	                               SIZE_LOG2_2MB, SIZE_LOG2_1GB>;
	using Ept_pdpt =
		Page_table_node<Ept_pd, Ept_page_directory_descriptor<SIZE_LOG2_1GB>,
		                SIZE_LOG2_1GB, SIZE_LOG2_512GB>;

}


struct Hw::Ept : Page_table_node<Ept_pdpt, Pml4e_table_descriptor,
                                 SIZE_LOG2_512GB, SIZE_LOG2_256TB>
{
	using Base = Page_table_node<Ept_pdpt, Pml4e_table_descriptor,
                                 SIZE_LOG2_512GB, SIZE_LOG2_256TB>;
	using Base::Base;
	using Array =
		Page_table_array<sizeof(Ept_pdpt),
	                     _table_count(_core_vm_size(), SIZE_LOG2_512GB,
	                                  SIZE_LOG2_1GB, SIZE_LOG2_2MB)>;
};

#endif /* _CORE__SPEC__PC__VIRTUALIZATION__EPT_H_ */
