/*
 * \brief  x86_64 translation table definitions for core
 * \author Adrian-Ken Rueegsegger
 * \date   2015-02-06
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SPEC__X86_64__TRANSLATION_TABLE_H_
#define _CORE__INCLUDE__SPEC__X86_64__TRANSLATION_TABLE_H_

/* Genode includes */
#include <util/misc_math.h>
#include <util/register.h>
#include <assert.h>

/* base-hw includes */
#include <util.h>
#include <page_flags.h>
#include <translation_table_allocator.h>

namespace Genode
{

	/**
	 * IA-32e paging translates 48-bit linear addresses to 52-bit physical
	 * addresses. Translation structures are hierarchical and four levels
	 * deep.
	 *
	 * For detailed information refer to Intel SDM Vol. 3A, section 4.5.
	 */

	enum {
		SIZE_LOG2_4KB   = 12,
		SIZE_LOG2_2MB   = 21,
		SIZE_LOG2_1GB   = 30,
		SIZE_LOG2_512GB = 39,
		SIZE_LOG2_256TB = 48,
	};

	class Level_4_translation_table;
	class Pml4_table;

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
	class Page_directory;

	using Level_3_translation_table =
		Page_directory<Level_4_translation_table,
		SIZE_LOG2_2MB, SIZE_LOG2_1GB>;
	using Level_2_translation_table =
		Page_directory<Level_3_translation_table,
		SIZE_LOG2_1GB, SIZE_LOG2_512GB>;

	/**
	 * IA-32e common descriptor.
	 *
	 * Table entry containing descriptor fields common to all four levels.
	 */
	struct Common_descriptor : Register<64>
	{
		struct P   : Bitfield<0, 1> { };   /* present         */
		struct Rw  : Bitfield<1, 1> { };   /* read/write      */
		struct Us  : Bitfield<2, 1> { };   /* user/supervisor */
		struct Pwt : Bitfield<3, 1> { };   /* write-through   */
		struct Pcd : Bitfield<4, 1> { };   /* cache disable   */
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

		/**
		 * Merge access rights of descriptor with given flags.
		 */
		static void merge_access_rights(access_t &desc,
		                                Page_flags const &flags)
		{
			Rw::set(desc, Rw::get(desc) | flags.writeable);
			Us::set(desc, Us::get(desc) | !flags.privileged);
			Xd::set(desc, Xd::get(desc) & !flags.executable);
		}
	};
}

class Genode::Level_4_translation_table
{
	private:

		static constexpr size_t PAGE_SIZE_LOG2 = SIZE_LOG2_4KB;
		static constexpr size_t SIZE_LOG2      = SIZE_LOG2_2MB;
		static constexpr size_t MAX_ENTRIES    = 1 << (SIZE_LOG2-PAGE_SIZE_LOG2);
		static constexpr size_t PAGE_SIZE      = 1 << PAGE_SIZE_LOG2;
		static constexpr size_t PAGE_MASK      = ~((1 << PAGE_SIZE_LOG2) - 1);

		class Misaligned {};
		class Invalid_range {};
		class Double_insertion {};

		struct Descriptor : Common_descriptor
		{
			using Common = Common_descriptor;

			struct Pat : Bitfield<7, 1> { };          /* page attribute table */
			struct G   : Bitfield<8, 1> { };          /* global               */
			struct Pa  : Bitfield<12, 36> { };        /* physical address     */
			struct Mt  : Bitset_3<Pwt, Pcd, Pat> { }; /* memory type          */

			static access_t create(Page_flags const &flags, addr_t const pa)
			{
				/* XXX: Set memory type depending on active PAT */
				return Common::create(flags)
					| G::bits(flags.global)
					| Pa::masked(pa);
			}
		};

		typename Descriptor::access_t _entries[MAX_ENTRIES];

		struct Insert_func
		{
			Page_flags const    & flags;
			Translation_table_allocator * alloc;

			Insert_func(Page_flags const & flags,
			            Translation_table_allocator * alloc)
			: flags(flags), alloc(alloc) { }

			void operator () (addr_t const vo, addr_t const pa,
			                  size_t const size,
			                  Descriptor::access_t &desc)
			{
				if ((vo & ~PAGE_MASK) || (pa & ~PAGE_MASK) ||
				    size < PAGE_SIZE)
				{
					throw Invalid_range();
				}
				Descriptor::access_t table_entry =
					Descriptor::create(flags, pa);

				if (Descriptor::present(desc) &&
				    Descriptor::clear_mmu_flags(desc) != table_entry)
				{
					throw Double_insertion();
				}
				desc = table_entry;
			}
		};

		struct Remove_func
		{
				Translation_table_allocator * alloc;

				Remove_func(Translation_table_allocator * alloc) : alloc(alloc) { }

				void operator () (addr_t const vo, addr_t const pa,
				                  size_t const size,
				                  Descriptor::access_t &desc)
				{ desc = 0; }
		};

		template <typename FUNC>
		void _range_op(addr_t vo, addr_t pa, size_t size, FUNC &&func)
		{
			for (size_t i = vo >> PAGE_SIZE_LOG2; size > 0;
			     i = vo >> PAGE_SIZE_LOG2) {
				addr_t end = (vo + PAGE_SIZE) & PAGE_MASK;
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

		static constexpr size_t MIN_PAGE_SIZE_LOG2 = SIZE_LOG2_4KB;
		static constexpr size_t ALIGNM_LOG2        = SIZE_LOG2_4KB;

		/**
		 * IA-32e page table (Level 4)
		 *
		 * A page table consists of 512 entries that each maps a 4KB page
		 * frame. For further details refer to Intel SDM Vol. 3A, table 4-19.
		 */
		Level_4_translation_table()
		{
			if (!aligned(this, ALIGNM_LOG2)) throw Misaligned();
			memset(&_entries, 0, sizeof(_entries));
		}

		/**
		 * Returns True if table does not contain any page mappings.
		 */
		bool empty()
		{
			for (unsigned i = 0; i < MAX_ENTRIES; i++)
				if (Descriptor::present(_entries[i]))
					return false;
			return true;
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
		void insert_translation(addr_t vo, addr_t pa, size_t size,
		                        Page_flags const & flags,
		                        Translation_table_allocator * alloc)
		{
			this->_range_op(vo, pa, size, Insert_func(flags, alloc));
		}

		/**
		 * Remove translations that overlap with a given virtual region
		 *
		 * \param vo    region offset within the tables virtual region
		 * \param size  region size
		 * \param alloc second level translation table allocator
		 */
		void remove_translation(addr_t vo, size_t size,
		                        Translation_table_allocator * alloc)
		{
			this->_range_op(vo, 0, size, Remove_func(alloc));
		}
} __attribute__((aligned(1 << ALIGNM_LOG2)));


template <typename ENTRY, unsigned PAGE_SIZE_LOG2, unsigned SIZE_LOG2>
class Genode::Page_directory
{
	private:

		static constexpr size_t MAX_ENTRIES = 1 << (SIZE_LOG2-PAGE_SIZE_LOG2);
		static constexpr size_t PAGE_SIZE   = 1 << PAGE_SIZE_LOG2;
		static constexpr size_t PAGE_MASK   = ~((1 << PAGE_SIZE_LOG2) - 1);

		class Misaligned {};
		class Invalid_range {};
		class Double_insertion {};

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
				/* XXX: Set memory type depending on active PAT */
				return Base::create(flags)
					| Base::Ps::bits(1)
					| G::bits(flags.global)
					| Pa::masked(pa);
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

			static typename Base::access_t create(Page_flags const &flags,
			                                      addr_t const pa)
			{
				/* XXX: Set memory type depending on active PAT */
				return Base::create(flags) | Pa::masked(pa);
			}
		};

		typename Base_descriptor::access_t _entries[MAX_ENTRIES];

		struct Insert_func
		{
			Page_flags const    & flags;
			Translation_table_allocator * alloc;

			Insert_func(Page_flags const & flags,
			            Translation_table_allocator * alloc)
			: flags(flags), alloc(alloc) { }

			void operator () (addr_t const vo, addr_t const pa,
			                  size_t const size,
			                  typename Base_descriptor::access_t &desc)
			{
				/* can we insert a large page mapping? */
				if (!((vo & ~PAGE_MASK) || (pa & ~PAGE_MASK) ||
				      size < PAGE_SIZE))
				{
					typename Base_descriptor::access_t table_entry =
						Page_descriptor::create(flags, pa);

					if (Base_descriptor::present(desc) &&
					    Base_descriptor::clear_mmu_flags(desc) != table_entry)
					{
						throw Double_insertion();
					}

					desc = table_entry;
					return;
				}

				/* we need to use a next level table */
				ENTRY *table;
				if (!Base_descriptor::present(desc)) {
					if (!alloc)
						throw Allocator::Out_of_memory();

					/* create and link next level table */
					table = new (alloc) ENTRY();
					ENTRY * phys_addr = (ENTRY*) alloc->phys_addr(table);
					addr_t const pa = (addr_t)(phys_addr ? phys_addr : table);
					desc = (typename Base_descriptor::access_t)
						Table_descriptor::create(flags, pa);

				} else if (Base_descriptor::maps_page(desc)) {
					throw Double_insertion();
				} else {
					Base_descriptor::merge_access_rights(desc, flags);
					ENTRY * phys_addr = (ENTRY*)
						Table_descriptor::Pa::masked(desc);
					table = (ENTRY*) alloc->virt_addr(phys_addr);
					table = table ? table : (ENTRY*)phys_addr;
				}

				/* insert translation */
				table->insert_translation(vo - (vo & PAGE_MASK),
			                          pa, size, flags, alloc);
			}
		};

		struct Remove_func
		{
				Translation_table_allocator * alloc;

				Remove_func(Translation_table_allocator * alloc) : alloc(alloc) { }

				void operator () (addr_t const vo, addr_t const pa,
				                  size_t const size,
				                  typename Base_descriptor::access_t &desc)
				{
					if (Base_descriptor::present(desc)) {
						if (Base_descriptor::maps_page(desc)) {
							desc = 0;
						} else {
							/* use allocator to retrieve virt address of table */
							ENTRY* phys_addr = (ENTRY*)
								Table_descriptor::Pa::masked(desc);
							ENTRY* table = (ENTRY*) alloc->virt_addr(phys_addr);
							table = table ? table : (ENTRY*)phys_addr;
							addr_t const table_vo = vo - (vo & PAGE_MASK);
							table->remove_translation(table_vo, size, alloc);
							if (table->empty()) {
								destroy(alloc, table);
								desc = 0;
							}
						}
					}
				}
		};

		template <typename FUNC>
		void _range_op(addr_t vo, addr_t pa, size_t size, FUNC &&func)
		{
			for (size_t i = vo >> PAGE_SIZE_LOG2; size > 0;
			     i = vo >> PAGE_SIZE_LOG2)
			{
				addr_t end = (vo + PAGE_SIZE) & PAGE_MASK;
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

		static constexpr size_t MIN_PAGE_SIZE_LOG2 = SIZE_LOG2_4KB;
		static constexpr size_t ALIGNM_LOG2        = SIZE_LOG2_4KB;

		Page_directory()
		{
			if (!aligned(this, ALIGNM_LOG2)) throw Misaligned();
			memset(&_entries, 0, sizeof(_entries));
		}

		/**
		 * Returns True if table does not contain any page mappings.
		 *
		 * \return   false if an entry is present, True otherwise
		 */
		bool empty()
		{
			for (unsigned i = 0; i < MAX_ENTRIES; i++)
				if (Base_descriptor::present(_entries[i]))
					return false;
			return true;
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
		void insert_translation(addr_t vo, addr_t pa, size_t size,
		                        Page_flags const & flags,
		                        Translation_table_allocator * alloc)
		{
			_range_op(vo, pa, size, Insert_func(flags, alloc));
		}

		/**
		 * Remove translations that overlap with a given virtual region
		 *
		 * \param vo    region offset within the tables virtual region
		 * \param size  region size
		 * \param alloc second level translation table allocator
		 */
		void remove_translation(addr_t vo, size_t size,
		                        Translation_table_allocator * alloc)
		{
			_range_op(vo, 0, size, Remove_func(alloc));
		}
} __attribute__((aligned(1 << ALIGNM_LOG2)));


class Genode::Pml4_table
{
	private:

		static constexpr size_t PAGE_SIZE_LOG2 = SIZE_LOG2_256TB;
		static constexpr size_t SIZE_LOG2      = SIZE_LOG2_512GB;
		static constexpr size_t MAX_ENTRIES    = 512;
		static constexpr size_t PAGE_SIZE      = 1UL << PAGE_SIZE_LOG2;
		static constexpr size_t PAGE_MASK      = ~((1UL << PAGE_SIZE_LOG2) - 1);

		class Misaligned {};
		class Invalid_range {};

		struct Descriptor : Common_descriptor
		{
			struct Pa  : Bitfield<12, SIZE_LOG2> { };  /* physical address */
			struct Mt  : Bitset_2<Pwt, Pcd> { };       /* memory type      */

			static access_t create(Page_flags const &flags, addr_t const pa)
			{
				/* XXX: Set memory type depending on active PAT */
				return Common_descriptor::create(flags) | Pa::masked(pa);
			}
		};

		typename Descriptor::access_t _entries[MAX_ENTRIES];

		using ENTRY = Level_2_translation_table;

		struct Insert_func
		{
				Page_flags const    & flags;
				Translation_table_allocator * alloc;

				Insert_func(Page_flags const & flags,
				            Translation_table_allocator * alloc)
				: flags(flags), alloc(alloc) { }

				void operator () (addr_t const vo, addr_t const pa,
				                  size_t const size,
				                  Descriptor::access_t &desc)
				{
					/* we need to use a next level table */
					ENTRY *table;
					if (!Descriptor::present(desc)) {
						if (!alloc)
							throw Allocator::Out_of_memory();

						/* create and link next level table */
						table = new (alloc) ENTRY();
						ENTRY * phys_addr = (ENTRY*) alloc->phys_addr(table);
						addr_t const pa = (addr_t)(phys_addr ? phys_addr : table);
						desc = Descriptor::create(flags, pa);
					} else {
						Descriptor::merge_access_rights(desc, flags);
						ENTRY * phys_addr = (ENTRY*)
							Descriptor::Pa::masked(desc);
						table = (ENTRY*) alloc->virt_addr(phys_addr);
						table = table ? table : (ENTRY*)phys_addr;
					}

					/* insert translation */
					addr_t const table_vo = vo - (vo & PAGE_MASK);
					table->insert_translation(table_vo, pa, size, flags, alloc);
				}
		};

		struct Remove_func
		{
				Translation_table_allocator * alloc;

				Remove_func(Translation_table_allocator * alloc) : alloc(alloc) { }

				void operator () (addr_t const vo, addr_t const pa,
				                  size_t const size,
				                  Descriptor::access_t &desc)
				{
					if (Descriptor::present(desc)) {
						/* use allocator to retrieve virt address of table */
						ENTRY* phys_addr = (ENTRY*)
							Descriptor::Pa::masked(desc);
						ENTRY* table = (ENTRY*) alloc->virt_addr(phys_addr);
						table = table ? table : (ENTRY*)phys_addr;
						addr_t const table_vo = vo - (vo & PAGE_MASK);
						table->remove_translation(table_vo, size, alloc);
						if (table->empty()) {
							destroy(alloc, table);
							desc = 0;
						}
					}
				}
		};

		template <typename FUNC>
		void _range_op(addr_t vo, addr_t pa, size_t size, FUNC &&func)
		{
			for (size_t i = vo >> PAGE_SIZE_LOG2; size > 0;
			     i = vo >> PAGE_SIZE_LOG2) {
				addr_t end = (vo + PAGE_SIZE) & PAGE_MASK;
				size_t sz  = min(size, end-vo);

				func(vo, pa, sz, _entries[i]);

				/* check whether we wrap */
				if (end < vo) return;

				size = size - sz;
				vo  += sz;
				pa  += sz;
			}
		}

	protected:

		/**
		 * Return how many entries of an alignment fit into region
		 */
		static constexpr size_t _count(size_t region, size_t alignment) {
			return align_addr<size_t>(region, alignment) / (1UL << alignment); }

	public:

		static constexpr size_t MIN_PAGE_SIZE_LOG2 = SIZE_LOG2_4KB;
		static constexpr size_t ALIGNM_LOG2        = SIZE_LOG2_4KB;

		Pml4_table()
		{
			if (!aligned(this, ALIGNM_LOG2)) throw Misaligned();
			memset(&_entries, 0, sizeof(_entries));
		}

		/**
		 * Returns True if table does not contain any page mappings.
		 *
		 * \return  false if an entry is present, True otherwise
		 */
		bool empty()
		{
			for (unsigned i = 0; i < MAX_ENTRIES; i++)
				if (Descriptor::present(_entries[i]))
					return false;
			return true;
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
		void insert_translation(addr_t vo, addr_t pa, size_t size,
		                        Page_flags const & flags,
		                        Translation_table_allocator * alloc)
		{
			_range_op(vo, pa, size, Insert_func(flags, alloc));
		}

		/**
		 * Remove translations that overlap with a given virtual region
		 *
		 * \param vo    region offset within the tables virtual region
		 * \param size  region size
		 * \param alloc second level translation table allocator
		 */
		void remove_translation(addr_t vo, size_t size,
		                        Translation_table_allocator * alloc)
		{
			_range_op(vo, 0, size, Remove_func(alloc));
		}
} __attribute__((aligned(1 << ALIGNM_LOG2)));


namespace Genode {
	class Translation_table;
}

class Genode::Translation_table : public Pml4_table
{
	public:

		enum {
			TABLE_LEVEL_X_SIZE_LOG2 = SIZE_LOG2_4KB,
			CORE_VM_AREA_SIZE       = 1024 * 1024 * 1024,
			CORE_TRANS_TABLE_COUNT  =
			_count(CORE_VM_AREA_SIZE, SIZE_LOG2_512GB)
			+ _count(CORE_VM_AREA_SIZE, SIZE_LOG2_1GB)
			+ _count(CORE_VM_AREA_SIZE, SIZE_LOG2_2MB)
		};
};

#endif /* _CORE__INCLUDE__SPEC__X86_64__TRANSLATION_TABLE_H_ */
