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

#include <util/misc_math.h>
#include <util/register.h>
#include <hw/page_flags.h>
#include <hw/page_table_allocator.h>

namespace Hw {

	enum {
		SIZE_LOG2_4KB   = 12,
		SIZE_LOG2_16KB  = 14,
		SIZE_LOG2_2MB   = 21,
		SIZE_LOG2_1GB   = 30,
		SIZE_LOG2_4GB   = 32,
		SIZE_LOG2_256GB = 38,
		SIZE_LOG2_512GB = 39,
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
		STAGE1, SIZE_LOG2_512GB>;

	using Level_3_stage_2_translation_table = Level_3_translation_table<STAGE2>;
	using Level_2_stage_2_translation_table =
		Level_x_translation_table<Level_3_stage_2_translation_table,
		STAGE2, SIZE_LOG2_1GB>;
	using Level_1_stage_2_translation_table =
		Level_x_translation_table<Level_2_stage_2_translation_table,
		STAGE2, SIZE_LOG2_256GB>;

	struct Page_table;

	using addr_t = Genode::addr_t;
	using size_t = Genode::size_t;
}


template <unsigned BLOCK_SIZE_LOG2, unsigned SZ_LOG2>
class Hw::Long_translation_table
{
	private:

		bool _aligned(addr_t const a, size_t const alignm_log2) {
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

		struct Descriptor : Genode::Register<64>
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
					UNCACHED = Genode::Cache_attribute::UNCACHED,
					CACHED   = Genode::Cache_attribute::CACHED,
					DEVICE
				};

				static typename Descriptor::access_t create(Page_flags const &f)
				{
					if (f.type == Hw::DEVICE)
						return Attribute_index::bits(DEVICE);

					switch (f.cacheable) {
					case Genode::CACHED:         return Attribute_index::bits(CACHED);
					case Genode::WRITE_COMBINED: [[fallthrough]];
					case Genode::UNCACHED:       return Attribute_index::bits(UNCACHED);
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

			struct Execute_never : Base::template Bitfield<54,1> { };

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
					| Descriptor::Valid::bits(1)
					| Execute_never::bits(!f.executable);
			}
		};


		struct Block_descriptor_stage2 : Block_descriptor_base
		{
			using Base = Block_descriptor_base;

			struct Mem_attr : Block_descriptor_base::template Bitfield<2,4>{};
			struct Hap      : Block_descriptor_base::template Bitfield<6,2>{};

			static typename Descriptor::access_t create(Page_flags const &,
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

			Genode::memset(&_entries, 0, sizeof(_entries));
		}

		template <typename FUNC>
		void _range_op(addr_t vo, addr_t pa, size_t size, FUNC &&func)
		{
			auto idx = [] (addr_t v) -> addr_t {
				return (v >> BLOCK_SIZE_LOG2) & (MAX_ENTRIES-1); };

			for (size_t i = idx(vo); size > 0; i = idx(vo)) {
				addr_t end = (vo + BLOCK_SIZE) & BLOCK_MASK;
				size_t sz  = Genode::min(size, end-vo);

				func(vo, pa, sz, _entries[i]);

				/* check whether we wrap */
				if (end < vo) return;

				size = size - sz;
				vo  += sz;
				pa  += sz;
			}
		}

        static inline void _update_cache(addr_t, size_t);

public:

		bool empty()
		{
			for (unsigned i = 0; i < MAX_ENTRIES; i++)
				if (Descriptor::valid(_entries[i]))
					return false;
			return true;
		}




} __attribute__((aligned(1 << ALIGNM_LOG2)));


template <Hw::Stage STAGE>
class Hw::Level_3_translation_table :
	public Hw::Long_translation_table<Hw::SIZE_LOG2_4KB,
	                                  Hw::SIZE_LOG2_2MB>
{
	private:

		struct Insert_func
		{
				Page_flags const & flags;

				Insert_func(Page_flags const & flags) : flags(flags) { }

				void operator () (addr_t const          vo,
				                  addr_t const          pa,
				                  size_t const          size,
				                  Descriptor::access_t &desc)
				{
					using Base = Long_translation_table<SIZE_LOG2_4KB,
					                                    SIZE_LOG2_2MB>;
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
                    Level_3_translation_table::_update_cache((addr_t) &desc, sizeof(desc));
				}
		};

		struct Remove_func
		{
				Remove_func() { }

				void operator () (addr_t /* vo */,
				                  addr_t /* pa */,
				                  size_t /* size */,
				                  Descriptor::access_t &desc) {
					desc = 0;
                    Level_3_translation_table::_update_cache((addr_t) &desc, sizeof(desc));
				}
		};

		struct Lookup_func
		{
				bool   found { false };
				addr_t phys  { 0 };

				Lookup_func() { }

				void operator () (addr_t const,
				                  addr_t const,
				                  size_t const,
				                  Descriptor::access_t &desc)
				{
					using Base = Long_translation_table<SIZE_LOG2_4KB,
					                                    SIZE_LOG2_2MB>;
					using Block_descriptor = typename Stage_trait<Base, STAGE>::Type;
					phys = Block_descriptor::Output_address::masked(desc);
					found = true;
				}
		};

	public:

		using Allocator = Hw::Page_table_allocator<1 << SIZE_LOG2_4KB>;

		void insert_translation(addr_t vo,
		                        addr_t pa,
		                        size_t size,
		                        Page_flags const & flags,
		                        Allocator &) {
			_range_op(vo, pa, size, Insert_func(flags));

            /* table descriptor cache update */
            Level_3_translation_table::_update_cache((addr_t) this, sizeof(this));
		}

		void remove_translation(addr_t vo, size_t size, Allocator&)
		{
			addr_t pa = 0;
			_range_op(vo, pa, size, Remove_func());

            /* table descriptor cache update */
            Level_3_translation_table::_update_cache((addr_t) this, sizeof(this));
		}

		bool lookup_translation(addr_t vo, addr_t & pa, Allocator&)
		{
			size_t page_size = 1 << SIZE_LOG2_4KB;
			Lookup_func functor {};
			_range_op(vo, 0, page_size, functor);
			pa = functor.phys;
			return functor.found;
		}
};


template <typename ENTRY, Hw::Stage STAGE, unsigned SIZE_LOG2>
class Hw::Level_x_translation_table :
	public Hw::Long_translation_table<ENTRY::SIZE_LOG2, SIZE_LOG2>
{
	public:

		using Allocator = Hw::Page_table_allocator<1 << SIZE_LOG2_4KB>;

	private:

		using Base = Long_translation_table<ENTRY::SIZE_LOG2, SIZE_LOG2>;
		using Descriptor       = typename Base::Descriptor;
		using Table_descriptor = typename Base::Table_descriptor;
		using Block_descriptor = typename Stage_trait<Base, STAGE>::Type;



		template <typename E>
		struct Insert_func
		{
			Page_flags const & flags;
			Allocator        & alloc;


			Insert_func(Page_flags const & flags, Allocator & alloc)
			: flags(flags), alloc(alloc) { }

			void operator () (addr_t const          vo,
			                  addr_t const          pa,
			                  size_t const          size,
			                  typename Descriptor::access_t &desc)
			{
				using Nt = typename Table_descriptor::Next_table;

				/* can we insert a whole block? */
				if (!((vo & ~Base::BLOCK_MASK) || (pa & ~Base::BLOCK_MASK) ||
				      size < Base::BLOCK_SIZE)) {
					typename Descriptor::access_t blk_desc =
						Block_descriptor::create(flags, pa);
					if (Descriptor::valid(desc) && desc != blk_desc)
						throw typename Base::Double_insertion();
					desc = blk_desc;
                    Level_x_translation_table::_update_cache((addr_t) &desc, sizeof(desc));
					return;
				}

				/* we need to use a next level table */
				switch (Descriptor::type(desc)) {

				case Descriptor::INVALID: /* no entry */
					{
						/* create and link next level table */
						E & table = alloc.construct<E>();
						desc = Table_descriptor::create((void*)alloc.phys_addr(table));
						Level_x_translation_table::_update_cache((addr_t) &desc, sizeof(desc));
					}
					[[fallthrough]];
				case Descriptor::TABLE: /* table already available */
					{
                        /* use allocator to retrieve virt address of table */
						E & table = alloc.virt_addr<E>(Nt::masked(desc));
						table.insert_translation(vo - (vo & Base::BLOCK_MASK),
						                         pa, size, flags, alloc);

						break;
					}

				case Descriptor::BLOCK: /* there is already a block */
					{
						throw typename Base::Double_insertion();
					}
				};
			}
		};


		template <typename E>
		struct Remove_func
		{
			Allocator & alloc;

			Remove_func(Allocator & alloc) : alloc(alloc) { }

			void operator () (addr_t const                   vo,
			                  addr_t const                /* pa */,
			                  size_t const                   size,
			                  typename Descriptor::access_t &desc)
			{
				using Nt = typename Table_descriptor::Next_table;

				switch (Descriptor::type(desc)) {
				case Descriptor::TABLE:
					{
						/* use allocator to retrieve virt address of table */
						E & table = alloc.virt_addr<E>(Nt::masked(desc));
						table.remove_translation(vo - (vo & Base::BLOCK_MASK),
						                         size, alloc);
						if (!table.empty()) break;
						alloc.destruct<E>(table);
					} [[fallthrough]];
				case Descriptor::BLOCK: [[fallthrough]];
				case Descriptor::INVALID:
					{
						desc = 0;
                        Level_x_translation_table::_update_cache((addr_t) &desc, sizeof(desc));
					}
				}
			}
		};


		template <typename E>
		struct Lookup_func
		{
			Allocator & alloc;
			bool        found { false };
			addr_t      phys  { 0 };

			Lookup_func(Allocator & alloc) : alloc(alloc) { }

			void operator () (addr_t const                   vo,
			                  addr_t const,
			                  size_t const,
			                  typename Descriptor::access_t &desc)
			{
				using Nt = typename Table_descriptor::Next_table;

				switch (Descriptor::type(desc)) {
				case Descriptor::BLOCK:
					{
						phys = Block_descriptor::Output_address::masked(desc);
						found = true;
						return;
					};
				case Descriptor::TABLE:
					{
						/* use allocator to retrieve virt address of table */
						E & table = alloc.virt_addr<E>(Nt::masked(desc));
						found = table.lookup_translation(vo - (vo & Base::BLOCK_MASK),
						                                 phys, alloc);
						return;
					};
				case Descriptor::INVALID: return;
				}
			}
		};




public:

		static constexpr size_t MIN_PAGE_SIZE_LOG2 = SIZE_LOG2_4KB;
		static constexpr size_t ALIGNM_LOG2        = SIZE_LOG2_16KB;

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
		void insert_translation(addr_t vo,
		                        addr_t pa,
		                        size_t size,
		                        Page_flags const & flags,
		                        Allocator        & alloc) {
			this->_range_op(vo, pa, size, Insert_func<ENTRY>(flags, alloc));

            /* table descriptor cache update */
            Level_x_translation_table::_update_cache((addr_t) this, sizeof(this));
		}

		/**
		 * Remove translations that overlap with a given virtual region
		 *
		 * \param vo     region offset within the tables virtual region
		 * \param size   region size
		 * \param alloc  second level translation table allocator
		 */
		void remove_translation(addr_t vo, size_t size,
		                        Allocator & alloc)
		{
			addr_t pa = 0;
			this->_range_op(vo, pa, size, Remove_func<ENTRY>(alloc));

            /* table descriptor cache update */
            Level_x_translation_table::_update_cache((addr_t) this, sizeof(this));
		}

		/**
		 * Lookup translation
		 *
		 * \param virt   region offset within the tables virtual region
		 * \param phys   region size
		 * \param alloc  second level translation table allocator
		 */
		bool lookup_translation(addr_t const virt, addr_t & phys,
		                        Allocator & alloc)
		{
			size_t page_size = 1 << SIZE_LOG2_4KB;
			Lookup_func<ENTRY> functor(alloc);
			this->_range_op(virt, phys, page_size, functor);
			phys = functor.phys;
			return functor.found;
		}

};


struct Hw::Page_table : Level_1_stage_1_translation_table
{
	enum {
		TABLE_LEVEL_X_SIZE_LOG2 = SIZE_LOG2_4KB,
		TABLE_LEVEL_X_ENTRIES   = (1 << SIZE_LOG2_4KB) / sizeof(addr_t),
		CORE_VM_AREA_SIZE       = 1024 * 1024 * 1024,
		SIZE_1GB                = 1 << SIZE_LOG2_1GB,
		CORE_LEVEL_2_TT_COUNT   = ((Genode::uint64_t)CORE_VM_AREA_SIZE +
		                           SIZE_1GB - 1) / SIZE_1GB,
		CORE_TRANS_TABLE_COUNT  = CORE_LEVEL_2_TT_COUNT +
		                          CORE_LEVEL_2_TT_COUNT *
		                          TABLE_LEVEL_X_ENTRIES,
	};

	Page_table() : Level_1_stage_1_translation_table() { }

	/**
	 * On ARM we do not need to copy top-level kernel entries
	 * because the virtual-memory kernel part is hold in a separate table
	 */
	explicit Page_table(Page_table &) : Level_1_stage_1_translation_table() { }
};
#endif /* _SRC__LIB__HW__SPEC__ARM__LPAE_H_ */
