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

#include <cpu/page_table_allocator.h>
#include <page_table/page_table_base.h>
#include <util/register.h>

namespace Hw {
	class Ept;
	using namespace Genode;

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
		static access_t clear_mmu_flags(access_t value)
		{
			A::clear(value);
			D::clear(value);
			return value;
		}
	};



	template <unsigned _PAGE_SIZE_LOG2, unsigned _SIZE_LOG2>
	struct Pml4e_table_descriptor : Ept_common_descriptor
	{
		static constexpr size_t PAGE_SIZE_LOG2 = _PAGE_SIZE_LOG2;
		static constexpr size_t SIZE_LOG2      = _SIZE_LOG2;

		struct Pa  : Bitfield<12, SIZE_LOG2> { };    /* Physical address */

		static access_t create(addr_t const pa)
		{
			/* XXX: Set memory type depending on active PAT */
			static Page_flags flags { RW, EXEC, USER, NO_GLOBAL,
				                  RAM, Genode::CACHED };
			return Ept_common_descriptor::create(flags) | Pa::masked(pa);
		}
	};

	struct Ept_page_directory_base_descriptor : Ept_common_descriptor
	{
		using Common = Ept_common_descriptor;

		struct Ps : Common::template Bitfield<7, 1> { };  /* Page size */

		static bool maps_page(access_t const v) { return Ps::get(v); }
	};


	template <unsigned _PAGE_SIZE_LOG2>
	struct Ept_page_directory_descriptor : Ept_page_directory_base_descriptor
	{
		static constexpr size_t PAGE_SIZE_LOG2 = _PAGE_SIZE_LOG2;

		struct Page;
		struct Table;
	};


	/* Table 29-7. Format of an EPT Page-Table Entry that Maps a 4-KByte Page */
	template <unsigned _PAGE_SIZE_LOG2>
	struct Ept_page_table_entry_descriptor : Ept_common_descriptor
	{
		using Common = Ept_common_descriptor;

		static constexpr size_t PAGE_SIZE_LOG2 = _PAGE_SIZE_LOG2;

		struct Type : Bitfield< 3, 3> { }; /* Ept memory type, see Section 29.3.7 */
		struct Pat  : Bitfield< 6, 1> { }; /* Ignore PAT memory type, see 29.3.7 */
		struct Pa   : Bitfield<12,36> { }; /* Physical address */

		static access_t create(Page_flags const &flags, addr_t const pa)
		{
			return Common::create(flags) | Pa::masked(pa) | Type::bits(6) | Pat::bits(1);
		}
	};

	struct Ept_page_table
	:
		Genode::Final_table<Ept_page_table_entry_descriptor<SIZE_LOG2_4KB>>
	{ } __attribute__((aligned(1 << ALIGNM_LOG2)));

	struct Ept_pd
	:
		Genode::Page_directory<Ept_page_table,
		Ept_page_directory_descriptor<SIZE_LOG2_2MB>>
	{ } __attribute__((aligned(1 << ALIGNM_LOG2)));

	struct Ept_pdpt
	:
		Genode::Page_directory<Ept_pd,
		Ept_page_directory_descriptor<SIZE_LOG2_1GB>>
	{ } __attribute__((aligned(1 << ALIGNM_LOG2)));

	struct Pml4e_table
	:
		Genode::Pml4_table<Ept_pdpt,
		Pml4e_table_descriptor<SIZE_LOG2_512GB, SIZE_LOG2_256TB>>
	{ } __attribute__((aligned(1 << ALIGNM_LOG2)));
}


template <unsigned _PAGE_SIZE_LOG2>
struct Hw::Ept_page_directory_descriptor<_PAGE_SIZE_LOG2>::Table
	: Ept_page_directory_base_descriptor
{
	using Base = Ept_page_directory_base_descriptor;

	/**
	 * Physical address
	 */
	struct Pa : Base::template Bitfield<12, 36> { };

	static typename Base::access_t create(addr_t const pa)
	{
		static Page_flags flags { RW, EXEC, USER, NO_GLOBAL,
		                          RAM, Genode::CACHED };
		return Base::create(flags) | Pa::masked(pa);
	}
};


template <unsigned _PAGE_SIZE_LOG2>
struct Hw::Ept_page_directory_descriptor<_PAGE_SIZE_LOG2>::Page
	: Ept_page_directory_base_descriptor
{
	using Base = Ept_page_directory_base_descriptor;

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
		     | Base::Ps::bits(1)
		     | Pa::masked(pa)
		     | Type::bits(6)
	             | Pat::bits(1);

	}
};

class Hw::Ept : public Pml4e_table
{
	public:
		using Allocator = Genode::Page_table_allocator<1UL << SIZE_LOG2_4KB>;
		using Pml4e_table::Pml4e_table;
};

#endif /* _CORE__SPEC__PC__VIRTUALIZATION__EPT_H_ */
