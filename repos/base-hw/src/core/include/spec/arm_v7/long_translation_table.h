/*
 * \brief   Long descriptor translation table definitions
 * \author  Stefan Kalkowski
 * \date    2014-07-07
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ARM_V7__LONG_TRANSLATION_TABLE_H_
#define _ARM_V7__LONG_TRANSLATION_TABLE_H_

/* Genode includes */
#include <util/misc_math.h>
#include <util/register.h>
#include <base/printf.h>

/* base-hw includes */
#include <page_flags.h>
#include <page_slab.h>

namespace Genode
{
	enum {
		SIZE_LOG2_4KB   = 12,
		SIZE_LOG2_16KB  = 14,
		SIZE_LOG2_2MB   = 21,
		SIZE_LOG2_1GB   = 30,
		SIZE_LOG2_4GB   = 32,
		SIZE_LOG2_256GB = 38,
	};

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
	class Long_translation_table;

	/**
	 * Translation table third level (leaf in the table tree)
	 *
	 * \param STAGE  marks whether it is part of a stage 1 or 2 table
	 */
	template <Stage STAGE>
	class Level_3_translation_table;

	/**
	 * Translation table first and second level
	 *
	 * \param ENTRY      type of the table entries
	 * \param STAGE      marks whether it is part of a stage 1 or 2 table
	 * \param SIZE_LOG2  virtual address range size in log2 of whole table
	 */
	template <typename ENTRY, Stage STAGE, unsigned SIZE_LOG2>
	class Level_x_translation_table;

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
		STAGE1, SIZE_LOG2_4GB>;

	using Level_3_stage_2_translation_table = Level_3_translation_table<STAGE2>;
	using Level_2_stage_2_translation_table =
		Level_x_translation_table<Level_3_stage_2_translation_table,
		STAGE2, SIZE_LOG2_1GB>;
	using Level_1_stage_2_translation_table =
		Level_x_translation_table<Level_2_stage_2_translation_table,
		STAGE2, SIZE_LOG2_256GB>;
}


template <unsigned BLOCK_SIZE_LOG2, unsigned SZ_LOG2>
class Genode::Long_translation_table
{
	private:

		inline bool _aligned(addr_t const a, size_t const alignm_log2) {
			return a == ((a >> alignm_log2) << alignm_log2); }

	public:

		static constexpr size_t SIZE_LOG2   = SZ_LOG2;
		static constexpr size_t MAX_ENTRIES = 1 << (SIZE_LOG2-BLOCK_SIZE_LOG2);
		static constexpr size_t BLOCK_SIZE  = 1 << BLOCK_SIZE_LOG2;
		static constexpr size_t BLOCK_MASK  = ~((1 << BLOCK_SIZE_LOG2) - 1);
		static constexpr size_t ALIGNM_LOG2 = SIZE_LOG2_4KB;

		class Misaligned {};
		class Invalid_range {};
		class Double_insertion {};

		struct Descriptor : Register<64>
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

			static bool valid(access_t const v) {
				return Valid::get(v); }
		};


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
				                              39 - BLOCK_SIZE_LOG2> { };

			/**
			 * Indicates that 16 adjacent entries point to contiguous
			 * memory regions
			 */
			struct Continguous_hint : Descriptor::template Bitfield<52,1> { };

			struct Execute_never : Descriptor::template Bitfield<54,1> { };
		};


		struct Table_descriptor : Descriptor
		{
			struct Next_table : Descriptor::template Bitfield<12, 27> { };

			static typename Descriptor::access_t create(void * const pa)
			{
				typename Descriptor::access_t oa = (addr_t) pa;
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
					UNCACHED = Cache_attribute::UNCACHED,
					CACHED   = Cache_attribute::CACHED,
					DEVICE
				};

				static typename Descriptor::access_t create(Page_flags const &f)
				{
					if (f.device) {  return Attribute_index::bits(DEVICE); }
					switch (f.cacheable) {
					case CACHED:         return Attribute_index::bits(CACHED);
					case WRITE_COMBINED:
					case UNCACHED:       return Attribute_index::bits(UNCACHED);
					}
					return 0;
				}
			};

			struct Non_secure : Base::template Bitfield<5, 1> { };

			struct Access_permission : Base::template Bitfield<6, 2>
			{
				enum { PRIVILEGED_RW, USER_RW, PRIVILEGED_RO, USER_RO };

				static typename Descriptor::access_t create(Page_flags const &f)
				{
					bool p = f.privileged;
					if (f.writeable) {
						if (p) { return Access_permission::bits(PRIVILEGED_RW); }
						else   { return Access_permission::bits(USER_RW);       }
					} else {
						if (p) { return Access_permission::bits(PRIVILEGED_RO); }
						else   { return Access_permission::bits(USER_RO);       }
					}
				}
			};

			struct Not_global : Base::template Bitfield<11,1> { };

			struct Privileged_execute_never : Base::template Bitfield<53,1> { };

			static typename Descriptor::access_t create(Page_flags const &f,
			                                            addr_t const pa)
			{
				return Access_permission::create(f)
					| Attribute_index::create(f)
					| Not_global::bits(!f.global)
					| Base::Shareability::bits(
						Base::Shareability::OUTER_SHAREABLE)
					| Base::Output_address::masked(pa)
					| Base::Access_flag::bits(1)
					| Descriptor::Valid::bits(1);
			}
		};


		struct Block_descriptor_stage2 : Block_descriptor_base
		{
			using Base = Block_descriptor_base;

			struct Mem_attr : Block_descriptor_base::template Bitfield<2,4>{};
			struct Hap      : Block_descriptor_base::template Bitfield<6,2>{};

			static typename Descriptor::access_t create(Page_flags const &f,
			                                            addr_t const pa)
			{
				return Base::Shareability::bits(
					Base::Shareability::NON_SHAREABLE)
					| Base::Output_address::masked(pa)
					| Base::Access_flag::bits(1)
					| Descriptor::Valid::bits(1)
					| Mem_attr::bits(0xf)
					| Hap::bits(0x3);
			}
		};

	protected:

		typename Descriptor::access_t _entries[MAX_ENTRIES];

		Long_translation_table()
		{
			if (!_aligned((addr_t)this, ALIGNM_LOG2))
				throw Misaligned();

			memset(&_entries, 0, sizeof(_entries));
		}

		template <typename FUNC>
		void _range_op(addr_t vo, addr_t pa, size_t size, FUNC &&func)
		{
			for (size_t i = vo >> BLOCK_SIZE_LOG2; size > 0;
			     i = vo >> BLOCK_SIZE_LOG2) {
				addr_t end = (vo + BLOCK_SIZE) & BLOCK_MASK;
				size_t sz  = min(size, end-vo);

				func(vo, pa, sz, _entries[i]);

				/* check whether we wrap */
				if (end < vo) return;

				size = size - sz;
				vo  += sz;
				pa  += sz;
			}
		}

	public:

		bool empty()
		{
			for (unsigned i = 0; i < MAX_ENTRIES; i++)
				if (Descriptor::valid(_entries[i]))
					return false;
			return true;
		}
} __attribute__((aligned(1 << ALIGNM_LOG2)));


template <Genode::Stage STAGE>
class Genode::Level_3_translation_table :
	public Genode::Long_translation_table<Genode::SIZE_LOG2_4KB,
	                                      Genode::SIZE_LOG2_2MB>
{
	private:

		struct Insert_func
		{
				Page_flags const & flags;
				Page_slab        * slab;

				Insert_func(Page_flags const & flags,
				            Page_slab * slab) : flags(flags), slab(slab) { }

				void operator () (addr_t const          vo,
				                  addr_t const          pa,
				                  size_t const          size,
				                  Descriptor::access_t &desc)
				{
					using Base = Long_translation_table<Genode::SIZE_LOG2_4KB,
					                                    Genode::SIZE_LOG2_2MB>;
					using Block_descriptor = typename Stage_trait<Base, STAGE>::Type;

					if ((vo & ~BLOCK_MASK) || (pa & ~BLOCK_MASK) ||
					    size < BLOCK_SIZE)
						throw Invalid_range();

					Descriptor::access_t blk_desc =
						Block_descriptor::create(flags, pa)
						| Descriptor::Table::bits(1);
					if (Descriptor::valid(desc) && desc != blk_desc)
						throw Double_insertion();
					desc = blk_desc;
				}
		};

		struct Remove_func
		{
				Page_slab * slab;

				Remove_func(Page_slab * slab) : slab(slab) { }

				void operator () (addr_t const          vo,
				                  addr_t const          pa,
				                  size_t const          size,
				                  Descriptor::access_t &desc) {
					desc = 0; }
		};

	public:

		void insert_translation(addr_t vo,
		                        addr_t pa,
		                        size_t size,
		                        Page_flags const & flags,
		                        Page_slab * slab) {
			_range_op(vo, pa, size, Insert_func(flags, slab)); }

		void remove_translation(addr_t vo, size_t size, Page_slab * slab) {
			_range_op(vo, 0, size, Remove_func(slab)); }
};


template <typename ENTRY, Genode::Stage STAGE, unsigned SIZE_LOG2>
class Genode::Level_x_translation_table :
	public Genode::Long_translation_table<ENTRY::SIZE_LOG2, SIZE_LOG2>
{
	private:

		using Base = Long_translation_table<ENTRY::SIZE_LOG2, SIZE_LOG2>;
		using Descriptor       = typename Base::Descriptor;
		using Table_descriptor = typename Base::Table_descriptor;
		using Block_descriptor = typename Stage_trait<Base, STAGE>::Type;

		struct Insert_func
		{
			Page_flags const & flags;
			Page_slab        * slab;

			Insert_func(Page_flags const & flags,
			            Page_slab * slab) : flags(flags), slab(slab) { }

			void operator () (addr_t const          vo,
			                  addr_t const          pa,
			                  size_t const          size,
			                  typename Descriptor::access_t &desc)
			{
				/* can we insert a whole block? */
				if (!((vo & ~Base::BLOCK_MASK) || (pa & ~Base::BLOCK_MASK) ||
				      size < Base::BLOCK_SIZE)) {
					typename Descriptor::access_t blk_desc =
						Block_descriptor::create(flags, pa);
					if (Descriptor::valid(desc) && desc != blk_desc)
						throw typename Base::Double_insertion();
					desc = blk_desc;
					return;
				}

				/* we need to use a next level table */
				ENTRY *table;
				switch (Descriptor::type(desc)) {

				case Descriptor::INVALID: /* no entry */
					{
						if (!slab) throw Allocator::Out_of_memory();

						/* create and link next level table */
						table = new (slab) ENTRY();
						ENTRY * phys_addr = (ENTRY*) slab->phys_addr(table);
						desc = Table_descriptor::create(phys_addr ?
						                                phys_addr : table);
					}

				case Descriptor::TABLE: /* table already available */
					{
						/* use allocator to retrieve virt address of table */
						ENTRY * phys_addr = (ENTRY*)
							Table_descriptor::Next_table::masked(desc);
						table = (ENTRY*) slab->virt_addr(phys_addr);
						table = table ? table : (ENTRY*)phys_addr;
						break;
					}

				case Descriptor::BLOCK: /* there is already a block */
					{
						throw typename Base::Double_insertion();
					}
				};

				/* insert translation */
				table->insert_translation(vo - (vo & Base::BLOCK_MASK),
				                          pa, size, flags, slab);
			}
		};


		struct Remove_func
		{
			Page_slab * slab;

			Remove_func(Page_slab * slab) : slab(slab) { }

			void operator () (addr_t const                   vo,
			                  addr_t const                   pa,
			                  size_t const                   size,
			                  typename Descriptor::access_t &desc)
			{
				switch (Descriptor::type(desc)) {
				case Descriptor::TABLE:
					{
						/* use allocator to retrieve virt address of table */
						ENTRY * phys_addr = (ENTRY*)
							Table_descriptor::Next_table::masked(desc);
						ENTRY * table = (ENTRY*) slab->virt_addr(phys_addr);
						table = table ? table : (ENTRY*)phys_addr;
						table->remove_translation(vo - (vo & Base::BLOCK_MASK),
						                          size, slab);
						if (!table->empty())
							break;
						destroy(slab, table);
					}
				case Descriptor::BLOCK:
				case Descriptor::INVALID:
					desc = 0;
				}
			}
		};

	public:

		static constexpr size_t MIN_PAGE_SIZE_LOG2 = SIZE_LOG2_4KB;
		static constexpr size_t ALIGNM_LOG2        = SIZE_LOG2_16KB;

		/**
		 * Insert translations into this table
		 *
		 * \param vo           offset of the virtual region represented
		 *                     by the translation within the virtual
		 *                     region represented by this table
		 * \param pa           base of the physical backing store
		 * \param size         size of the translated region
		 * \param flags        mapping flags
		 * \param slab  second level page slab allocator
		 */
		void insert_translation(addr_t vo,
		                        addr_t pa,
		                        size_t size,
		                        Page_flags const & flags,
		                        Page_slab * slab) {
			this->_range_op(vo, pa, size, Insert_func(flags, slab)); }

		/**
		 * Remove translations that overlap with a given virtual region
		 *
		 * \param vo    region offset within the tables virtual region
		 * \param size  region size
		 * \param slab  second level page slab allocator
		 */
		void remove_translation(addr_t vo, size_t size, Page_slab * slab) {
			this->_range_op(vo, 0, size, Remove_func(slab)); }
};

namespace Genode {
	class Translation_table : public Level_1_stage_1_translation_table { }; }

#endif /* _ARM_V7__LONG_TRANSLATION_TABLE_H_ */
