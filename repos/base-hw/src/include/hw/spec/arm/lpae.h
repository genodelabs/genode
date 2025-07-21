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

	/**
	 * The stage indicates the kind of translation
	 *
	 * STAGE1 denotes the translation of 32-bit virtual addresses
	 *        to a 40-bit "intermediate" or actual physical addresses
	 * STAGE2 denotes the translation of intermediate physical
	 *        addresses to actual physical addresses
	 */
	enum Stage { STAGE1, STAGE2 };

	/**
	 * Translation table base class for long format
	 *
	 * Long translation tables are part of the Large Physical Address Extension
	 * of ARM. They are organized in at most three-level. This class comprises
	 * properties shared between all 3 level of tables.
	 *
	 * \param BLOCK_SIZE_LOG2  virtual address range size in log2
	 *                         of a single table entry
	 * \param SIZE_LOG2        virtual address range size in log2 of whole table
	 */
	template <unsigned BLOCK_SIZE_LOG2, unsigned SIZE_LOG2>
	struct Long_translation_table;

	/**
	 * Translation table third level (leaf in the table tree)
	 *
	 * \param STAGE  marks whether it is part of a stage 1 or 2 table
	 */
	template <Stage STAGE>
	struct Level_3_translation_table;

	/**
	 * Translation table first and second level
	 *
	 * \param ENTRY      type of the table entries
	 * \param STAGE      marks whether it is part of a stage 1 or 2 table
	 * \param SIZE_LOG2  virtual address range size in log2 of whole table
	 */
	template <typename ENTRY, Stage STAGE, unsigned SIZE_LOG2>
	struct Level_x_translation_table;

	/**
	 * Trait pattern: helper class to carry information about
	 *                the type of a block descriptor (and stage)
	 *                through the template forest
	 *
	 * \param TABLE  carries information about the table type
	 * \param STAGE  marks whether it is part of a stage 1 or 2 table
	 */
	template <typename TABLE, Stage STAGE> struct Stage_trait;

	template <typename TABLE> struct Stage_trait<TABLE, STAGE1> {
		using Type = typename TABLE::Block_descriptor_stage1; };
	template <typename TABLE> struct Stage_trait<TABLE, STAGE2> {
		using Type = typename TABLE::Block_descriptor_stage2; };

	using Level_3_stage_1_translation_table = Level_3_translation_table<STAGE1>;
	using Level_2_stage_1_translation_table =
		Level_x_translation_table<Level_3_stage_1_translation_table,
		STAGE1, SIZE_LOG2_1GB>;
	using Level_1_stage_1_translation_table =
		Level_x_translation_table<Level_2_stage_1_translation_table,
		STAGE1, SIZE_LOG2_512GB>;

	using Level_3_stage_2_translation_table = Level_3_translation_table<STAGE2>;
	using Level_2_stage_2_translation_table =
		Level_x_translation_table<Level_3_stage_2_translation_table,
		STAGE2, SIZE_LOG2_1GB>;
	using Level_1_stage_2_translation_table =
		Level_x_translation_table<Level_2_stage_2_translation_table,
		STAGE2, SIZE_LOG2_256GB>;

	struct Page_table;
	struct Page_table_descriptor;
}


struct Hw::Page_table_descriptor : Genode::Register<64>
{
	enum Type { INVALID, TABLE, BLOCK };

	struct Valid : Bitfield<0, 1> { };
	struct Table : Bitfield<1, 1> { };

	static Type type(access_t const v)
	{
		if (!Valid::get(v)) return INVALID;
		if (Table::get(v))  return TABLE;
		return BLOCK;
	}

	static bool present(access_t const v) {
		return Valid::get(v); }
};


template <unsigned BLOCK_SIZE_LOG2, unsigned SZ_LOG2>
struct Hw::Long_translation_table
: Page_table_tpl<Page_table_descriptor, BLOCK_SIZE_LOG2, SZ_LOG2>
{
	static constexpr size_t SIZE_LOG2   = SZ_LOG2;
	static constexpr size_t ALIGNM_LOG2 = SIZE_LOG2_4KB;

	using Descriptor = Page_table_descriptor;
	using desc_t     = typename Descriptor::access_t;

	struct Block_descriptor_base : Descriptor
	{
		struct Shareability : Descriptor::template Bitfield<8, 2>
		{
			enum { NON_SHAREABLE   = 0,
			       OUTER_SHAREABLE = 2,
			       INNER_SHAREABLE = 3 };
		};

		struct Access_flag : Descriptor::template Bitfield<10,1> { };

		struct Output_address :
			Descriptor::template Bitfield<BLOCK_SIZE_LOG2,
			                              47 - BLOCK_SIZE_LOG2> { };

		/**
		 * Indicates that 16 adjacent entries point to contiguous
		 * memory regions
		 */
		struct Continguous_hint : Descriptor::template Bitfield<52,1> { };

		struct Execute_never : Descriptor::template Bitfield<54,1> { };
	};


	struct Table_descriptor : Descriptor
	{
		struct Next_table : Descriptor::template Bitfield<12, 36> { };

		static desc_t create(addr_t pa)
		{
			desc_t oa = pa;
			return Next_table::masked(oa)
				| Descriptor::Table::bits(1)
				| Descriptor::Valid::bits(1);
		}
	};


	struct Block_descriptor_stage1 : Block_descriptor_base
	{
		using Base = Block_descriptor_base;

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

			static desc_t create(Page_flags const &f)
			{
				if (f.type == Genode::DEVICE)
					return Attribute_index::bits(DEVICE);

				switch (f.cacheable) {
				case Genode::CACHED:
					return Attribute_index::bits(CACHED);
				case Genode::WRITE_COMBINED:
					[[fallthrough]];
				case Genode::UNCACHED:
					return Attribute_index::bits(UNCACHED);
				}
				return 0;
			}
		};

		struct Non_secure : Base::template Bitfield<5, 1> { };

		struct Access_permission : Base::template Bitfield<6, 2>
		{
			enum { PRIVILEGED_RW, USER_RW, PRIVILEGED_RO, USER_RO };

			static desc_t create(Page_flags const &f)
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

		static desc_t create(Page_flags const &f, addr_t const pa)
		{
			return Access_permission::create(f)
				| Attribute_index::create(f)
				| Not_global::bits(!f.global)
				| Base::Shareability::bits(
					Base::Shareability::INNER_SHAREABLE)
				| Base::Output_address::masked(pa)
				| Base::Access_flag::bits(1)
				| Descriptor::Valid::bits(1)
				| Execute_never::bits(!f.executable);
		}

		static bool writeable(desc_t desc)
		{
			typename Base::access_t ap =
				Access_permission::get(desc);

			if (!Descriptor::present(desc) ||
			    (ap != Access_permission::PRIVILEGED_RW &&
			     ap != Access_permission::USER_RW))
				return false;
			return true;
		}
	};


	struct Block_descriptor_stage2 : Block_descriptor_base
	{
		using Base = Block_descriptor_base;

		struct Mem_attr : Block_descriptor_base::template Bitfield<2,4>{};
		struct Hap      : Block_descriptor_base::template Bitfield<6,2>{};

		static desc_t create(Page_flags const &, addr_t const pa)
		{
			return Base::Shareability::bits(
				Base::Shareability::INNER_SHAREABLE)
				| Base::Output_address::masked(pa)
				| Base::Access_flag::bits(1)
				| Descriptor::Valid::bits(1)
				| Mem_attr::bits(0xf)
				| Hap::bits(0x3);
		}
	};
};


template <Hw::Stage STAGE>
struct Hw::Level_3_translation_table
: Hw::Long_translation_table<Hw::SIZE_LOG2_4KB, Hw::SIZE_LOG2_2MB>
{
	using Base = Long_translation_table<SIZE_LOG2_4KB, SIZE_LOG2_2MB>;
	using Block_descriptor = typename Stage_trait<Base, STAGE>::Type;

	Result insert(addr_t vo, addr_t pa, size_t size,
	              Page_flags const &flags, Page_table_allocator&)
	{
		return Base::_for_range(vo, pa, size,
			[&] (addr_t vo, addr_t pa, size_t size, desc_t &desc) -> Result
			{
				if (!Base::_aligned_and_fits(vo, pa, size))
					return Page_table_error::INVALID_RANGE;

				desc_t blk_desc = Block_descriptor::create(flags, pa)
				                  | Descriptor::Table::bits(1);

				if (Descriptor::present(desc) && desc != blk_desc)
					return Page_table_error::INVALID_RANGE;

				desc = blk_desc;
				return Ok();
			});
	}

	void remove(addr_t vo, size_t size, Page_table_allocator&)
	{
		return Base::_for_range(vo, size, [&] (addr_t, size_t, desc_t &desc) {
			desc = 0; });
	}

	Result lookup(addr_t vo, addr_t &pa, Page_table_allocator&)
	{
		return Base::_for_range(vo, 0, 1,
			[&] (addr_t, addr_t, size_t, desc_t &desc) -> Result
			{
				using Bd = Block_descriptor;

				pa = (addr_t) Bd::Output_address::masked(desc);

				if (!Bd::writeable(desc))
					return Page_table_error::INVALID_RANGE;
				return Ok();
			});
	}
};


template <typename ENTRY, Hw::Stage STAGE, unsigned SIZE_LOG2>
struct Hw::Level_x_translation_table
: Hw::Long_translation_table<ENTRY::SIZE_LOG2, SIZE_LOG2>
{
	using Base = Long_translation_table<ENTRY::SIZE_LOG2, SIZE_LOG2>;
	using Descriptor       = Base::Descriptor;
	using desc_t           = typename Descriptor::access_t;
	using Table_descriptor = typename Base::Table_descriptor;
	using Block_descriptor = typename Stage_trait<Base, STAGE>::Type;
	using Result           = Base::Result;

	/**
	 * Insert translations into this table
	 *
	 * \param vo      offset of the virtual region represented
	 *                by the translation within the virtual
	 *                region represented by this table
	 * \param pa      base of the physical backing store
	 * \param size    size of the translated region
	 * \param flags   mapping flags
	 * \param alloc   second level translation table allocator
	 */
	Result insert(addr_t vo, addr_t pa, size_t size, Page_flags const &flags,
	              Page_table_allocator &alloc)
	{
		return Base::_for_range(vo, pa, size,
			[&] (addr_t vo, addr_t pa, size_t size, desc_t &desc)
			-> Result {
				using Nt = typename Table_descriptor::Next_table;

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
						Result result =
							alloc.create<ENTRY, Table_descriptor>(desc);
						if (result.failed())
							return result;
					}
					[[fallthrough]];
				case Descriptor::TABLE: /* table already available */
					/**
					 * Use allocator to retrieve virt address of table
					 * (we do not have physical memory above 4G on 32bit
					 *  yet, therefore we can downcast here)
					 */
					return alloc.lookup<ENTRY>((addr_t)Nt::masked(desc),
					                           [&] (ENTRY &table) {
						return table.insert(vo-Base::_page_mask_high(vo),
						                    pa, size, flags, alloc);
					});
				case Descriptor::BLOCK: /* there is already a block */
					break;
				};
				return Page_table_error::INVALID_RANGE;
			});
	}

	/**
	 * Remove translations that overlap with a given virtual region
	 *
	 * \param vo     region offset within the tables virtual region
	 * \param size   region size
	 * \param alloc  second level translation table allocator
	 */
	void remove(addr_t vo, size_t size, Page_table_allocator &alloc)
	{
		Base::_for_range(vo, size,
			[&] (addr_t vo, size_t size, desc_t &desc)
			{
				using Nt = typename Table_descriptor::Next_table;

				switch (Descriptor::type(desc)) {
				case Descriptor::TABLE:
					alloc.lookup<ENTRY>((addr_t)Nt::masked(desc),
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
	}

	/**
	 * Lookup writeable translation
	 *
	 * \param virt   region offset within the tables virtual region
	 * \param phys   region size
	 * \param alloc  second level translation table allocator
	 */
	Result lookup(addr_t const virt, addr_t &phys, Page_table_allocator &alloc)
	{
		return Base::_for_range(virt, 0, 1,
			[&] (addr_t vo, addr_t, size_t, desc_t &desc) -> Result
			{
				using Bd = Block_descriptor;
				using Nt = typename Table_descriptor::Next_table;

				switch (Descriptor::type(desc)) {
				case Descriptor::BLOCK:
					{
						/* downcast: no phys memory above 4G on 32bit yet */
						phys = (addr_t)Bd::Output_address::masked(desc);

						if (!Bd::writeable(desc))
							return Page_table_error::INVALID_RANGE;
						return Ok();
					};
				case Descriptor::TABLE:
					/*
					 * Use allocator to retrieve virt address of table
					 * (we do not have physical memory above 4G on 32bit
					 *  yet, therefore we can downcast here)
					 */
					return alloc.lookup<ENTRY>((addr_t)Nt::masked(desc),
					                           [&] (ENTRY &table) {
						return table.lookup(vo-Base::_page_mask_high(vo),
						                    phys, alloc);
					});
				case Descriptor::INVALID: break;
				}

				return Page_table_error::INVALID_RANGE;
			});
	}
};


struct Hw::Page_table : Level_1_stage_1_translation_table
{
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
} __attribute__((aligned(1 << ALIGNM_LOG2)));

#endif /* _SRC__LIB__HW__SPEC__ARM__LPAE_H_ */
