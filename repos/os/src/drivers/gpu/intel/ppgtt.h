/*
 * \brief  Broadwell graphics translation table definitions
 * \author Josef Soentgen
 * \date   2017-03-15
 *
 * Adapted copy of base-hw's IA-32e translation table by
 * Adrian-Ken Rueegsegger et. al.
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PPGTT_H_
#define _PPGTT_H_

/* Genode includes */
#include <base/allocator.h>
#include <base/cache.h>
#include <base/output.h>
#include <util/misc_math.h>
#include <util/register.h>

/* local includes */
#include <utils.h>

namespace Genode
{
	/**
	 * Return an address rounded down to a specific alignment
	 *
	 * \param addr         original address
	 * \param alignm_log2  log2 of the required alignment
	 */
	inline Gpu::addr_t trunc(Gpu::addr_t const addr, Gpu::addr_t const alignm_log2) {
		return (addr >> alignm_log2) << alignm_log2; }

	/**
	 * Translation table allocator interface
	 */
	class Translation_table_allocator : public Genode::Allocator
	{
		public:

			/**
			 * Return physical address of given virtual page address
			 *
			 * \param addr  virtual page address
			 */
			virtual void * phys_addr(void * addr) = 0;

			/**
			 * Return virtual address of given physical page address
			 *
			 * \param addr  physical page address
			 */
			virtual void * virt_addr(void * addr) = 0;
	};

	enum Writeable   { RO, RW            };
	enum Executeable { NO_EXEC, EXEC     };
	enum Privileged  { USER, KERN        };
	enum Global      { NO_GLOBAL, GLOBAL };
	enum Type        { RAM, DEVICE       };

	struct Page_flags
	{
		Writeable writeable;
	};

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
	 * Table entry contains descriptor fields common to all four levels.
	 *
	 * IHD-OS-BDW-Vol 5-11.15 p. 23 ff.
	 */
	struct Common_descriptor : Register<64>
	{
		struct P   : Bitfield< 0, 1> { };  /* present         */ /* must always be 1, see scratch pages */
		struct Rw  : Bitfield< 1, 1> { };  /* read/write      */ /* not supported on Gen8 */
		struct Us  : Bitfield< 2, 1> { };  /* user/supervisor */ /* not supported on Gen8 */
		struct Pwt : Bitfield< 3, 1> { };  /* write-through   */ /* Pwt and Pcd are used as index into PAT */
		struct Pcd : Bitfield< 4, 1> { };  /* cache disable   */ /* see Mmio::PAT_INDEX */
		struct A   : Bitfield< 5, 1> { };  /* accessed        */
		struct D   : Bitfield< 6, 1> { };  /* dirty           */
		struct Xd  : Bitfield<63, 1> { };  /* execute-disable */ /* not supported on Gen8 */

		static bool present(access_t const v) { return P::get(v); }

		static access_t create(Page_flags const &)
		{
			return P::bits(1) | Rw::bits(1);
		}

		/**
		 * Merge access rights of descriptor with given flags.
		 */
		static void merge_access_rights(access_t &desc,
		                                Page_flags const &flags)
		{
			Rw::set(desc, Rw::get(desc) | flags.writeable);
		}
	};

	struct Scratch
	{
		private:

			Utils::Backend_alloc &_backend;

		public:

			enum {
				PAGE_SIZE   = 4096,
				MAX_ENTRIES = 512,
			};

			using Ram = Utils::Ram;

			struct Page
			{
				Genode::Ram_dataspace_capability ds { };
				Gpu::addr_t  addr = 0;
				Page   *next = nullptr;
			};

			Page page { };
			Page pt   { };
			Page pd   { };
			Page pdp  { };

			Scratch(Utils::Backend_alloc &backend)
			:
				_backend(backend)
			{
				/* XXX addr PAT helper instead of hardcoding */
				page.ds    = _backend.alloc(PAGE_SIZE);
				page.addr  = _backend.dma_addr(page.ds);
				page.addr |= 1;
				page.addr |= 1 << 1;
				page.next  = nullptr;

				pt.ds      = _backend.alloc(PAGE_SIZE);
				pt.addr    = _backend.dma_addr(pt.ds);
				pt.addr   |= 1;
				pt.addr   |= 1 << 1;
				pt.next    = &page;

				pd.ds      = _backend.alloc(PAGE_SIZE);
				pd.addr    = _backend.dma_addr(pd.ds);
				pd.addr   |= 1;
				pd.addr   |= 1 << 1;
				pd.next    = &pt;

				pdp.ds     = _backend.alloc(PAGE_SIZE);
				pdp.addr   = _backend.dma_addr(pdp.ds);
				pdp.addr  |= 1;
				pdp.addr  |= 1 << 1;
				pdp.next   = &pd;
			}

			virtual ~Scratch()
			{
				_backend.free(pdp.ds);
				_backend.free(pd.ds);
				_backend.free(pt.ds);
				_backend.free(page.ds);
			}
	};
}


class Genode::Level_4_translation_table
{
	public:

		class Misaligned {};
		class Invalid_address {};
		class Invalid_range {};
		class Double_insertion {};

	private:

		static constexpr size_t PAGE_SIZE_LOG2 = SIZE_LOG2_4KB;
		static constexpr size_t SIZE_LOG2      = SIZE_LOG2_2MB;
		static constexpr size_t MAX_ENTRIES    = 1UL << (SIZE_LOG2-PAGE_SIZE_LOG2);
		static constexpr size_t PAGE_SIZE      = 1UL << PAGE_SIZE_LOG2;
		static constexpr size_t PAGE_MASK      = ~((1UL << PAGE_SIZE_LOG2) - 1);

		struct Descriptor : Common_descriptor
		{
			using Common = Common_descriptor;

			struct Pat : Bitfield<7, 1> { };          /* page attribute table */
			struct Pa  : Bitfield<12, 36> { };        /* physical address     */

			static access_t create(Page_flags const &flags, Gpu::addr_t const pa)
			{
				/* XXX: Set memory type depending on active PAT */
				return Common::create(flags) | Pat::bits(1) | Pa::masked(pa);
			}

			static bool scratch(typename Descriptor::access_t desc,
			                    Gpu::addr_t const scratch_addr)
			{
				return Pa::masked(desc) == Pa::masked(scratch_addr);
			}
		};

		typename Descriptor::access_t _entries[MAX_ENTRIES];

		struct Insert_func
		{
			Page_flags const            &flags;
			Translation_table_allocator *alloc;
			Scratch::Page               *scratch;

			Insert_func(Page_flags const            &flags,
			            Translation_table_allocator *alloc,
			            Scratch::Page               *scratch)
			: flags(flags), alloc(alloc), scratch(scratch) { }

			void operator () (Gpu::addr_t const vo, Gpu::addr_t const pa,
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

				/* only complain if we overmap */
				if (   !Descriptor::scratch(desc, scratch->addr)
				    && !Descriptor::scratch(desc, pa)) {
					throw Double_insertion();
				}
				desc = table_entry;
			}
		};

		struct Remove_func
		{
				Translation_table_allocator *alloc;
				Scratch::Page               *scratch;

				Remove_func(Translation_table_allocator *alloc,
				            Scratch::Page               *scratch)
				: alloc(alloc), scratch(scratch) { }

				void operator () (Gpu::addr_t /* vo */, Gpu::addr_t /* pa */,
				                  size_t /* size */,
				                  Descriptor::access_t &desc)
				{
					desc = scratch->addr;
				}
		};

		template <typename FUNC>
		void _range_op(Gpu::addr_t vo, Gpu::addr_t pa, size_t size, FUNC &&func)
		{
			for (size_t i = vo >> PAGE_SIZE_LOG2; size > 0;
			     i = vo >> PAGE_SIZE_LOG2) {
				Gpu::addr_t end = (vo + PAGE_SIZE) & PAGE_MASK;
				size_t sz  = min(size, end-vo);

				if (i >= MAX_ENTRIES)
					throw Invalid_address();

				func(vo, pa, sz, _entries[i]);
				Utils::clflush(&_entries[i]);

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
		Level_4_translation_table(Scratch::Page *scratch)
		{
			if (!aligned((Gpu::addr_t)this, ALIGNM_LOG2)) { throw Misaligned(); }

			for (size_t i = 0; i < MAX_ENTRIES; i++) {
				_entries[i] = scratch->addr;
			}
		}

		/**
		 * Returns True if table does not contain any page mappings.
		 */
		bool empty(Gpu::addr_t scratch_addr)
		{
			for (unsigned i = 0; i < MAX_ENTRIES; i++) {
				if (!Descriptor::scratch(_entries[i], scratch_addr)) {
					return false;
				}
			}
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
		void insert_translation(Gpu::addr_t vo, Gpu::addr_t pa, size_t size,
		                        Page_flags const & flags,
		                        Translation_table_allocator *alloc,
		                        Scratch::Page               *scratch)
		{
			_range_op(vo, pa, size, Insert_func(flags, alloc, scratch));
		}

		/**
		 * Remove translations that overlap with a given virtual region
		 *
		 * \param vo    region offset within the tables virtual region
		 * \param size  region size
		 * \param alloc second level translation table allocator
		 */
		void remove_translation(Gpu::addr_t vo, size_t size,
		                        Translation_table_allocator *alloc,
		                        Scratch::Page               *scratch)
		{
			_range_op(vo, 0, size, Remove_func(alloc, scratch));
		}
} __attribute__((aligned(1 << ALIGNM_LOG2)));


template <typename ENTRY, unsigned PAGE_SIZE_LOG2, unsigned SIZE_LOG2>
class Genode::Page_directory
{
	private:

		static constexpr size_t MAX_ENTRIES = 1UL << (SIZE_LOG2-PAGE_SIZE_LOG2);
		static constexpr size_t PAGE_SIZE   = 1UL << PAGE_SIZE_LOG2;
		static constexpr size_t PAGE_MASK   = ~((1UL << PAGE_SIZE_LOG2) - 1);

		class Misaligned {};
		class Invalid_address {};
		class Invalid_range {};
		class Double_insertion {};

		struct Base_descriptor : Common_descriptor
		{
			using Common = Common_descriptor;
		};

		struct Page_descriptor : Base_descriptor
		{
			using Base = Base_descriptor;

			/**
			 * Physical address
			 */
			struct Pa : Base::template Bitfield<12, 36> { };

			static typename Base::access_t create(Page_flags const &flags,
			                                      Gpu::addr_t const pa)
			{
				/* XXX: Set memory type depending on active PAT */
				return Base::create(flags) | Pa::masked(pa);
			}

			static bool scratch(typename Page_descriptor::access_t desc,
			                    Gpu::addr_t const scratch_addr)
			{
				return Pa::masked(desc) == Pa::masked(scratch_addr);
			}
		};

		struct Table_descriptor : Base_descriptor
		{
			using Base = Base_descriptor;

			/**
			 * Physical address
			 */
			struct Pa : Base::template Bitfield<12, 36> { };

			static typename Base::access_t create(Page_flags const &flags,
			                                      Gpu::addr_t const pa)
			{
				/* XXX: Set memory type depending on active PAT */
				return Base::create(flags) | Pa::masked(pa);
			}
		};

		typename Base_descriptor::access_t _entries[MAX_ENTRIES];

		struct Insert_func
		{
			Page_flags const            &flags;
			Translation_table_allocator *alloc;
			Scratch::Page               *scratch;

			Insert_func(Page_flags const            &flags,
			            Translation_table_allocator *alloc,
			            Scratch::Page               *scratch)
			: flags(flags), alloc(alloc), scratch(scratch) { }

			void operator () (Gpu::addr_t const vo, Gpu::addr_t const pa,
			                  size_t const size,
			                  typename Base_descriptor::access_t &desc)
			{
				/* we need to use a next level table */
				ENTRY *table;
				if (Page_descriptor::scratch(desc, scratch->addr)) {
					if (!alloc) { throw Allocator::Out_of_memory(); }

					/* create and link next level table */
					table = new (alloc) ENTRY(scratch->next);
					ENTRY * phys_addr = (ENTRY*) alloc->phys_addr(table);

					Gpu::addr_t const pa = (Gpu::addr_t)(phys_addr ? phys_addr : table);
					desc = (typename Base_descriptor::access_t) Table_descriptor::create(flags, pa);

				} else {
					Base_descriptor::merge_access_rights(desc, flags);
					ENTRY * phys_addr = (ENTRY*) Table_descriptor::Pa::masked(desc);
					table = (ENTRY*) alloc->virt_addr(phys_addr);
					if (!table) { table = (ENTRY*) phys_addr; }
				}

				/* insert translation */
				Gpu::addr_t const table_vo = vo - (vo & PAGE_MASK);
				table->insert_translation(table_vo, pa, size, flags, alloc, scratch->next);
			}
		};

		struct Remove_func
		{
				Translation_table_allocator *alloc;
				Scratch::Page               *scratch;

				Remove_func(Translation_table_allocator *alloc,
							Scratch::Page               *scratch)
				: alloc(alloc), scratch(scratch) { }

				void operator () (Gpu::addr_t const vo, Gpu::addr_t /* pa */,
				                  size_t const size,
				                  typename Base_descriptor::access_t &desc)
				{
					if (!Page_descriptor::scratch(desc, scratch->addr)) {
						/* use allocator to retrieve virt address of table */
						ENTRY *phys_addr = (ENTRY*)
							Table_descriptor::Pa::masked(desc);
						ENTRY *table = (ENTRY*) alloc->virt_addr(phys_addr);
						if (!table) { table = (ENTRY*) phys_addr; }

						Gpu::addr_t const table_vo = vo - (vo & PAGE_MASK);
						table->remove_translation(table_vo, size, alloc, scratch->next);
						if (table->empty(scratch->next->addr)) {
							destroy(alloc, table);
							desc = scratch->addr;
						}
					}
				}
		};

		template <typename FUNC>
		void _range_op(Gpu::addr_t vo, Gpu::addr_t pa, size_t size, FUNC &&func)
		{
			for (size_t i = vo >> PAGE_SIZE_LOG2; size > 0;
			     i = vo >> PAGE_SIZE_LOG2)
			{
				Gpu::addr_t end = (vo + PAGE_SIZE) & PAGE_MASK;
				size_t sz  = min(size, end-vo);

				if (i >= MAX_ENTRIES)
					throw Invalid_address();

				func(vo, pa, sz, _entries[i]);
				Utils::clflush(&_entries[i]);

				/* check whether we wrap */
				if (end < vo) { return; }

				size = size - sz;
				vo  += sz;
				pa  += sz;
			}
		}

	public:

		static constexpr size_t MIN_PAGE_SIZE_LOG2 = SIZE_LOG2_4KB;
		static constexpr size_t ALIGNM_LOG2        = SIZE_LOG2_4KB;

		Page_directory(Scratch::Page *scratch)
		{
			if (!aligned((Gpu::addr_t)this, ALIGNM_LOG2)) { throw Misaligned(); }

			for (size_t i = 0; i < MAX_ENTRIES; i++) {
				_entries[i] = scratch->addr;
			}
		}

		/**
		 * Returns True if table does not contain any page mappings.
		 *
		 * \return   false if an entry is present, True otherwise
		 */
		bool empty(Gpu::addr_t scratch_addr)
		{
			for (unsigned i = 0; i < MAX_ENTRIES; i++) {
				if (!Page_descriptor::scratch(_entries[i], scratch_addr)) {
					return false;
				}
			}
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
		void insert_translation(Gpu::addr_t vo, Gpu::addr_t pa, size_t size,
		                        Page_flags const & flags,
		                        Translation_table_allocator *alloc,
		                        Scratch::Page               *scratch)
		{
			_range_op(vo, pa, size, Insert_func(flags, alloc, scratch));
		}

		/**
		 * Remove translations that overlap with a given virtual region
		 *
		 * \param vo    region offset within the tables virtual region
		 * \param size  region size
		 * \param alloc second level translation table allocator
		 */
		void remove_translation(Gpu::addr_t vo, size_t size,
		                        Translation_table_allocator *alloc,
		                        Scratch::Page               *scratch)
		{
			_range_op(vo, 0, size, Remove_func(alloc, scratch));
		}
} __attribute__((aligned(1 << ALIGNM_LOG2)));


class Genode::Pml4_table
{
	private:

		static constexpr size_t PAGE_SIZE_LOG2 = SIZE_LOG2_512GB;
		static constexpr size_t SIZE_LOG2      = SIZE_LOG2_256TB;
		static constexpr size_t MAX_ENTRIES    = 512;
		static constexpr uint64_t PAGE_SIZE    = 1ULL << PAGE_SIZE_LOG2;
		static constexpr uint64_t PAGE_MASK    = ~((1ULL << PAGE_SIZE_LOG2) - 1);

		class Misaligned {};
		class Invalid_address {};
		class Invalid_range {};

		struct Descriptor : Common_descriptor
		{
			struct Pa : Bitfield<12, 36> { };    /* physical address */

			static access_t create(Page_flags const &flags, Gpu::addr_t const pa)
			{
				/* XXX: Set memory type depending on active PAT */
				return Common_descriptor::create(flags) | Pa::masked(pa);
			}

			static bool scratch(Descriptor::access_t desc, Gpu::addr_t const pa)
			{
				return Pa::masked(desc) == Pa::masked(pa);
			}
		};

		typename Descriptor::access_t _entries[MAX_ENTRIES];

		using ENTRY = Level_2_translation_table;

		struct Insert_func
		{
				Page_flags const            &flags;
				Translation_table_allocator *alloc;
				Scratch::Page               *scratch;

				Insert_func(Page_flags const            &flags,
				            Translation_table_allocator *alloc,
				            Scratch::Page               *scratch)
				: flags(flags), alloc(alloc), scratch(scratch) { }

				void operator () (Gpu::addr_t const vo, Gpu::addr_t const pa,
				                  size_t const size,
				                  Descriptor::access_t &desc)
				{
					/* we need to use a next level table */
					ENTRY *table;
					if (Descriptor::scratch(desc, scratch->addr)) {
						if (!alloc) { throw Allocator::Out_of_memory(); }

						/* create and link next level table */
						table = new (alloc) ENTRY(scratch->next);

						ENTRY * phys_addr = (ENTRY*) alloc->phys_addr(table);
						Gpu::addr_t const pa = (Gpu::addr_t)(phys_addr ? phys_addr : table);
						desc = Descriptor::create(flags, pa);
					} else {
						Descriptor::merge_access_rights(desc, flags);
						ENTRY * phys_addr = (ENTRY*) Descriptor::Pa::masked(desc);
						table = (ENTRY*) alloc->virt_addr(phys_addr);
						if (!table) { table = (ENTRY*) phys_addr; }
					}

					/* insert translation */
					Gpu::addr_t const table_vo = vo - (vo & PAGE_MASK);
					table->insert_translation(table_vo, pa, size, flags, alloc, scratch->next);
				}
		};

		struct Remove_func
		{
				Translation_table_allocator *alloc;
				Scratch::Page               *scratch;

				Remove_func(Translation_table_allocator *alloc,
				            Scratch::Page               *scratch)
				: alloc(alloc), scratch(scratch) { }

				void operator () (Gpu::addr_t const vo, Gpu::addr_t /* pa */,
				                  size_t const size,
				                  Descriptor::access_t &desc)
				{
					if (!Descriptor::scratch(desc, scratch->addr)) {

						ENTRY* phys_addr = (ENTRY*) Descriptor::Pa::masked(desc);
						ENTRY *table = (ENTRY*) alloc->virt_addr(phys_addr);
						if (!table) { table = (ENTRY*) phys_addr; }

						Gpu::addr_t const table_vo = vo - (vo & PAGE_MASK);
						table->remove_translation(table_vo, size, alloc, scratch->next);
						if (table->empty(scratch->next->addr)) {
							destroy(alloc, table);
							desc = scratch->addr;
						}
					}
				}
		};

		template <typename FUNC>
		void _range_op(Gpu::addr_t vo, Gpu::addr_t pa, size_t size, FUNC &&func)
		{
			for (size_t i = vo >> PAGE_SIZE_LOG2; size > 0;
			     i = vo >> PAGE_SIZE_LOG2) {

				Gpu::addr_t const end = (vo + PAGE_SIZE) & PAGE_MASK;
				size_t const sz  = min(size, end-vo);

				if (i >= MAX_ENTRIES)
					throw Invalid_address();

				func(vo, pa, sz, _entries[i]);
				Utils::clflush(&_entries[i]);

				/* check whether we wrap */
				if (end < vo) { return; }

				size = size - sz;
				vo  += sz;
				pa  += sz;
			}
		}

	public:

		static constexpr size_t MIN_PAGE_SIZE_LOG2 = SIZE_LOG2_4KB;
		static constexpr size_t ALIGNM_LOG2        = SIZE_LOG2_4KB;

		Pml4_table(Scratch::Page *scratch)
		{
			if (!aligned((Gpu::addr_t)this, ALIGNM_LOG2)) { throw Misaligned(); }

			for (size_t i = 0; i < MAX_ENTRIES; i++) {
				_entries[i] = scratch->addr;
			}
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
		void insert_translation(Gpu::addr_t vo, Gpu::addr_t pa, size_t size,
		                        Page_flags const & flags,
		                        Translation_table_allocator *alloc,
		                        Scratch::Page *scratch)
		{
			_range_op(vo, pa, size, Insert_func(flags, alloc, scratch));
		}

		/**
		 * Remove translations that overlap with a given virtual region
		 *
		 * \param vo    region offset within the tables virtual region
		 * \param size  region size
		 * \param alloc second level translation table allocator
		 */
		void remove_translation(Gpu::addr_t vo, size_t size,
		                        Translation_table_allocator *alloc,
		                        Scratch::Page *scratch)
		{
			_range_op(vo, 0, size, Remove_func(alloc, scratch));
		}
} __attribute__((aligned(1 << ALIGNM_LOG2)));


namespace Igd {

	struct Ppgtt;
	struct Ppgtt_scratch;
}


/**
 * Per-process graphics translation table
 */
struct Igd::Ppgtt : public Genode::Pml4_table
{
	Ppgtt(Genode::Scratch::Page *scratch) : Pml4_table(scratch) { }
};


/**
 * Per-process graphics translation table scratch pages
 */
struct Igd::Ppgtt_scratch : public Genode::Scratch
{
		Ppgtt_scratch(Utils::Backend_alloc &backend)
		: Scratch(backend) { }
};

#endif /* _PPGTT_H_ */
