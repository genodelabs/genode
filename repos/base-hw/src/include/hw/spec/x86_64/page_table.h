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
	struct Page_table_4;
	struct Page_table;

	/**
	 * IA-32e page directory template.
	 *
	 * Page directories can refer to paging structures of the next higher level
	 * or directly map page frames by using large page mappings.
	 *
	 * \param PAGE_SIZE_LOG2  virtual address range size in log2
	 *                        of a single table entry
	 * \param SIZE_LOG2       virtual address range size in log2 of whole table
	 */
	template <typename ENTRY, unsigned PAGE_SIZE_LOG2, unsigned SIZE_LOG2>
	struct Page_directory;

	using Page_table_3 =
		Page_directory<Page_table_4, SIZE_LOG2_2MB, SIZE_LOG2_1GB>;

	using Page_table_2 =
		Page_directory<Page_table_3, SIZE_LOG2_1GB, SIZE_LOG2_512GB>;


	/**
	 * IA-32e common descriptor.
	 *
	 * Table entry containing descriptor fields common to all four levels.
	 */
	struct Common_descriptor : Genode::Register<64>
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
		static access_t clear_mmu_flags(access_t value)
		{
			A::clear(value);
			D::clear(value);
			return value;
		}
	};
}


/**
 * IA-32e page table (Level 4)
 *
 * A page table consists of 512 entries that each maps a 4KB page
 * frame. For further details refer to Intel SDM Vol. 3A, table 4-19.
 */
struct Hw::Page_table_4
: Page_table_tpl<Common_descriptor, SIZE_LOG2_4KB, SIZE_LOG2_2MB>
{
	struct Descriptor : Common_descriptor
	{
		using Common = Common_descriptor;

		struct Pat : Bitfield<7, 1> { };          /* page attribute table */
		struct G   : Bitfield<8, 1> { };          /* global               */
		struct Pa  : Bitfield<12, 36> { };        /* physical address     */
		struct Mt  : Genode::Bitset_3<Pwt, Pcd, Pat> { }; /* memory type  */

		static access_t create(Page_flags const &flags, addr_t const pa)
		{
			bool const wc = flags.cacheable == Cache::WRITE_COMBINED;

			return Common::create(flags)
				| G::bits(flags.global)
				| Pa::masked(pa)
				| Pwt::bits(wc ? 1 : 0);
		}
	};

	using Base = Page_table_tpl<Common_descriptor, SIZE_LOG2_4KB,
	                            SIZE_LOG2_2MB>;
	using Result = Page_table_insertion_result;


	/**
	 * Insert translations into this table
	 *
	 * \param vo     offset of the virtual region represented
	 *               by the translation within the virtual
	 *               region represented by this table
	 * \param pa     base of the physical backing store
	 * \param size   size of the translated region
	 * \param flags  mapping flags
	 * \param alloc  second level translation table allocator
	 */
	Result insert(addr_t vo, addr_t pa, size_t size,
	              Page_flags const &flags, Page_table_allocator&)
	{
		using desc_t = typename Descriptor::access_t;

		return Base::_for_range(vo, pa, size,
			[&] (addr_t const vo, addr_t const pa,
			     size_t const size, desc_t &desc) -> Result
			{
				if (!Base::_aligned_and_fits(vo, pa, size))
					return Page_table_error::INVALID_RANGE;

				desc_t table_entry = Descriptor::create(flags, pa);

				if (Descriptor::present(desc) &&
				    Descriptor::clear_mmu_flags(desc) != table_entry)
					return Page_table_error::INVALID_RANGE;

				desc = table_entry;
				return Ok();
			});
	}


	/**
	 * Remove translations that overlap with a given virtual region
	 *
	 * \param vo    region offset within the tables virtual region
	 * \param size  region size
	 * \param alloc second level translation table allocator
	 */
	void remove(addr_t vo, size_t size, Page_table_allocator&)
	{
		using desc_t = typename Descriptor::access_t;

		Base::_for_range(vo, size, [] (addr_t, size_t, desc_t &d) {
			d = 0; });
	}
};


template <typename ENTRY, unsigned PAGE_SIZE_LOG2, unsigned SIZE_LOG2>
struct Hw::Page_directory
: Page_table_tpl<Common_descriptor, PAGE_SIZE_LOG2, SIZE_LOG2>
{
	struct Base_descriptor : Common_descriptor
	{
		using Common = Common_descriptor;

		struct Ps : Common::template Bitfield<7, 1> { };  /* page size */

		static bool maps_page(access_t const v) { return Ps::get(v); }
	};

	struct Page_descriptor : Base_descriptor
	{
		using Base = Base_descriptor;

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

		/**
		 * Memory type
		 */
		struct Mt : Base::template Bitset_3<Base::Pwt,
		                                    Base::Pcd, Pat> { };

		static typename Base::access_t create(Page_flags const &flags,
		                                      addr_t const pa)
		{
			bool const wc =
				flags.cacheable == Genode::Cache::WRITE_COMBINED;

			return Base::create(flags)
			     | Base::Ps::bits(1)
			     | G::bits(flags.global)
			     | Pa::masked(pa)
			     | Base::Pwt::bits(wc ? 1 : 0);
		}
	};

	struct Table_descriptor : Base_descriptor
	{
		using Base = Base_descriptor;

		/**
		 * Physical address
		 */
		struct Pa : Base::template Bitfield<12, 36> { };

		/**
		 * Memory types
		 */
		struct Mt : Base::template Bitset_2<Base::Pwt,
		                                    Base::Pcd> { };

		static typename Base::access_t create(addr_t const pa)
		{
			/* XXX: Set memory type depending on active PAT */
			static Page_flags flags { RW, EXEC, USER, NO_GLOBAL,
			                          RAM, Genode::CACHED };
			return Base::create(flags) | Pa::masked(pa);
		}
	};

	using Base =
		Page_table_tpl<Common_descriptor, PAGE_SIZE_LOG2, SIZE_LOG2>;
	using Result = Page_table_insertion_result;

	/**
	 * Insert translations into this table
	 *
	 * \param vo     offset of the virtual region represented
	 *               by the translation within the virtual
	 *               region represented by this table
	 * \param pa     base of the physical backing store
	 * \param size   size of the translated region
	 * \param flags  mapping flags
	 * \param alloc  second level translation table allocator
	 */
	Result insert(addr_t vo, addr_t pa, size_t size,
	              Page_flags const &flags,
	              Page_table_allocator &alloc)
	{
		using Descriptor = Base_descriptor;
		using desc_t     = typename Descriptor::access_t;
		using Td         = Table_descriptor;

		return Base::_for_range(vo, pa, size,
			[&] (addr_t const vo, addr_t const pa,
			     size_t const size, desc_t &desc) -> Result
			{
				/* can we insert a large page mapping? */
				if (Base::_aligned_and_fits(vo, pa, size)) {
					desc_t table_entry =
						Page_descriptor::create(flags, pa);

					if (Descriptor::present(desc) &&
					    Descriptor::clear_mmu_flags(desc) != table_entry)
						return Page_table_error::INVALID_RANGE;

					desc = table_entry;
					return Ok();
				}

				if (Descriptor::present(desc) &&
				    Descriptor::maps_page(desc))
					return Page_table_error::INVALID_RANGE;

				/* do we need to create and link next level table? */
				if (!Descriptor::present(desc)) {
				    Result result = alloc.create<ENTRY, Td>(desc);
					if (result.failed())
						return result;
				}

				return alloc.lookup<ENTRY>(Td::Pa::masked(desc),
				                           [&] (ENTRY &table) {
					return table.insert(vo-Base::_page_mask_high(vo),
					                    pa, size, flags, alloc);
				});
			});
	}

	/**
	 * Remove translations that overlap with a given virtual region
	 *
	 * \param vo    region offset within the tables virtual region
	 * \param size  region size
	 * \param alloc second level translation table allocator
	 */
	void remove(addr_t vo, size_t size, Page_table_allocator &alloc)
	{
		Base::_for_range(vo, size, [&] (addr_t const vo, size_t const size,
		                          typename Base_descriptor::access_t &desc)
		{
			if (!Base_descriptor::present(desc))
				return;

			if (Base_descriptor::maps_page(desc)) {
				desc = 0;
				return;
			}

			using Td = Table_descriptor;

			alloc.lookup<ENTRY>(Td::Pa::masked(desc), [&] (ENTRY &table) {
				table.remove(vo-Base::_page_mask_high(vo), size, alloc);
				if (table.empty()) {
					alloc.destroy<ENTRY>(table);
					desc = 0;
				}
				return Ok();
			}).with_error([] (Page_table_error) {
				/* ignore non-mapped entries */ });
		});
	}
};


struct Hw::Page_table
: Page_table_tpl<Common_descriptor, SIZE_LOG2_512GB, SIZE_LOG2_256TB>
{
	struct Descriptor : Common_descriptor
	{
		struct Pa  : Bitfield<12, SIZE_LOG2_256TB> { }; /* physical address */
		struct Mt  : Genode::Bitset_2<Pwt, Pcd> { }; /* memory type      */

		static access_t create(addr_t const pa)
		{
			/* XXX: Set memory type depending on active PAT */
			static Page_flags flags { RW, EXEC, USER, NO_GLOBAL,
			                          RAM, Genode::CACHED };
			return Common_descriptor::create(flags) | Pa::masked(pa);
		}
	};

	using ENTRY = Page_table_2;

	static constexpr size_t ALIGNM_LOG2 = SIZE_LOG2_4KB;
	static constexpr size_t SIZE_MASK   = (1UL << SIZE_LOG2_256TB) - 1;

	using Base =
		Page_table_tpl<Common_descriptor, SIZE_LOG2_512GB, SIZE_LOG2_256TB>;
	using typename Base::Result;

	using Base::Base;

	explicit Page_table(Page_table &kernel_table) : Page_table()
	{
		static constexpr size_t KERNEL_START_IDX =
			(Hw::Mm::KERNEL_START & SIZE_MASK) >> SIZE_LOG2_512GB;

		/* copy kernel page directories to new table */
		for (size_t i = KERNEL_START_IDX; i < Base::MAX_ENTRIES; i++)
			_entries[i] = kernel_table._entries[i];
	}

	/**
	 * Insert translations into this table
	 *
	 * \param vo     offset of the virtual region represented
	 *               by the translation within the virtual
	 *               region represented by this table
	 * \param pa     base of the physical backing store
	 * \param size   size of the translated region
	 * \param flags  mapping flags
	 * \param alloc  second level translation table allocator
	 */
	Result insert(addr_t vo, addr_t pa, size_t size,
	              Page_flags const &flags, Page_table_allocator &alloc)
	{
		return Base::_for_range(vo, pa, size,
			[&] (addr_t const vo, addr_t const pa, size_t const size,
			     Descriptor::access_t &desc) -> Result
			{
				if (!Descriptor::present(desc)) {
					Result result = alloc.create<ENTRY, Descriptor>(desc);
					if (result.failed())
						return result;
				}

				return alloc.lookup<ENTRY>(Descriptor::Pa::masked(desc),
				                           [&] (ENTRY &table) {
					addr_t const table_vo = vo-Base::_page_mask_high(vo);
					return table.insert(table_vo, pa, size, flags, alloc);
				});
			});
	}

	/**
	 * Remove translations that overlap with a given virtual region
	 *
	 * \param vo    region offset within the tables virtual region
	 * \param size  region size
	 * \param alloc second level translation table allocator
	 */
	void remove(addr_t vo, size_t size, Page_table_allocator &alloc)
	{
		Base::_for_range(vo, size, [&] (addr_t const vo, size_t const size,
		                               Descriptor::access_t &desc) {
			if (!Descriptor::present(desc))
				return;

			alloc.lookup<ENTRY>(Descriptor::Pa::masked(desc),
			                    [&] (ENTRY &table) {
				table.remove(vo-Base::_page_mask_high(vo), size, alloc);
				if (table.empty()) {
					alloc.destroy<ENTRY>(table);
					desc = 0;
				}
				return Ok();
			}).with_error([&] (Page_table_error) {
				/* Deleting non-existent page-table entry is okay */ });
		});
	}

	Result lookup(addr_t const, addr_t&, Page_table_allocator&)
	{
		Genode::error(__func__, " not implemented yet");
		return Page_table_error::INVALID_RANGE;
	}

	using Array =
		Page_table_array<sizeof(Page_table_2),
	                     _table_count(_core_vm_size(), SIZE_LOG2_512GB,
	                                  SIZE_LOG2_1GB, SIZE_LOG2_2MB)>;
} __attribute__((aligned(1 << ALIGNM_LOG2)));

#endif /* _SRC__LIB__HW__SPEC__X86_64__PAGE_TABLE_H_ */
