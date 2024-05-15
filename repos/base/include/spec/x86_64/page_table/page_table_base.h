/*
 * \brief  x86_64 page table definitions
 * \author Adrian-Ken Rueegsegger
 * \author Johannes Schlatow
 * \author Benjamin Lamowski
 * \date   2015-02-06
 */

/*
 * Copyright (C) 2015-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__X86_64__PAGE_TABLE__PAGE_TABLE_BASE_H_
#define _INCLUDE__SPEC__X86_64__PAGE_TABLE__PAGE_TABLE_BASE_H_

#include <base/log.h>
#include <cpu/page_flags.h>
#include <util/misc_math.h>
#include <cpu/clflush.h>

namespace Genode {

	/**
	 * (Generic) 4-level translation structures.
	 */

	enum {
		SIZE_LOG2_4KB   = 12,
		SIZE_LOG2_2MB   = 21,
		SIZE_LOG2_1GB   = 30,
		SIZE_LOG2_512GB = 39,
		SIZE_LOG2_256TB = 48,
	};

	/**
	 * Final page table template
	 *
	 * The last-level page table solely maps page frames.
	 */
	template <typename DESCRIPTOR>
	class Final_table;

	/**
	 * Page directory template.
	 *
	 * Page directories can refer to paging structures of the next level
	 * or directly map page frames by using large page mappings.
	 */
	template <typename ENTRY, typename DESCRIPTOR>
	class Page_directory;

	/**
	 * The 4th-level table refers to paging structures of the next level.
	 */
	template <typename ENTRY, typename DESCRIPTOR>
	class Pml4_table;
}


template <typename DESCRIPTOR>
class Genode::Final_table
{
	public:

		using Descriptor = DESCRIPTOR;

	private:

		static constexpr size_t PAGE_SIZE_LOG2 = DESCRIPTOR::PAGE_SIZE_LOG2;
		static constexpr size_t MAX_ENTRIES    = 512;
		static constexpr size_t PAGE_SIZE      = 1UL << PAGE_SIZE_LOG2;
		static constexpr size_t PAGE_MASK      = ~((1UL << PAGE_SIZE_LOG2) - 1);

		class Misaligned {};
		class Invalid_range {};
		class Double_insertion {};

		typename DESCRIPTOR::access_t _entries[MAX_ENTRIES];

		struct Insert_func
		{
			Page_flags const    & flags;
			bool                  flush;

			Insert_func(Page_flags const & flags, bool flush)
			: flags(flags), flush(flush) { }

			void operator () (addr_t const vo, addr_t const pa,
			                  size_t const size,
			                  DESCRIPTOR::access_t &desc)
			{
				if ((vo & ~PAGE_MASK) || (pa & ~PAGE_MASK) ||
				    size < PAGE_SIZE)
				{
					throw Invalid_range();
				}
				typename DESCRIPTOR::access_t table_entry =
					DESCRIPTOR::create(flags, pa);

				if (DESCRIPTOR::present(desc) &&
				    DESCRIPTOR::clear_mmu_flags(desc) != table_entry)
				{
					throw Double_insertion();
				}
				desc = table_entry;

				if (flush)
					clflush(&desc);
			}
		};

		struct Remove_func
		{
			bool flush;

			Remove_func(bool flush) : flush(flush) { }

			void operator () (addr_t /* vo */, addr_t /* pa */, size_t /* size */,
			                  DESCRIPTOR::access_t &desc)
			{
				desc = 0;

				if (flush)
					clflush(&desc);
			}
		};

		template <typename FUNC>
		void _range_op(addr_t vo, addr_t pa, size_t size, FUNC &&func)
		{
			for (size_t i = vo >> PAGE_SIZE_LOG2; size > 0;
			     i = vo >> PAGE_SIZE_LOG2) {
				addr_t end = (vo + PAGE_SIZE) & PAGE_MASK;
				size_t sz  = Genode::min(size, end-vo);

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
		 * A page table consists of 512 entries that each maps a 4KB page
		 * frame. For further details refer to Intel SDM Vol. 3A, table 4-19.
		 */
		Final_table()
		{
			if (!aligned<addr_t>((addr_t)this, ALIGNM_LOG2)) throw Misaligned();
			Genode::memset(&_entries, 0, sizeof(_entries));
		}

		/**
		 * Returns True if table does not contain any page mappings.
		 */
		bool empty()
		{
			for (unsigned i = 0; i < MAX_ENTRIES; i++)
				if (DESCRIPTOR::present(_entries[i]))
					return false;
			return true;
		}

		template <typename FN>
		void for_each_entry(FN && fn)
		{
			for (unsigned long i = 0; i < MAX_ENTRIES; i++) {
				if (Descriptor::present(_entries[i]))
					fn(i, _entries[i]);
			}
		}

		/**
		 * Insert translations into this table
		 *
		 * \param vo               offset of the virtual region represented
		 *                         by the translation within the virtual
		 *                         region represented by this table
		 * \param pa               base of the physical backing store
		 * \param size             size of the translated region
		 * \param flags            mapping flags
		 * \param alloc            second level translation table allocator
		 * \param flush            flush cache lines of table entries
		 * \param supported_sizes  supported page sizes
		 */
		template <typename ALLOCATOR>
		void insert_translation(addr_t vo, addr_t pa, size_t size,
		                        Page_flags const & flags, ALLOCATOR &,
		                        bool flush = false, uint32_t supported_sizes = (1U << 30 | 1U << 21 | 1U << 12))
		{
			(void)supported_sizes;
			this->_range_op(vo, pa, size, Insert_func(flags, flush));
		}

		/**
		 * Remove translations that overlap with a given virtual region
		 *
		 * \param vo    region offset within the tables virtual region
		 * \param size  region size
		 * \param alloc second level translation table allocator
		 */
		template <typename ALLOCATOR>
		void remove_translation(addr_t vo, size_t size, ALLOCATOR &, bool flush = false)
		{
			this->_range_op(vo, 0, size, Remove_func(flush));
		}

} __attribute__((aligned(1 << ALIGNM_LOG2)));


template <typename ENTRY, typename DESCRIPTOR>
class Genode::Page_directory
{
	public:

		using Descriptor = DESCRIPTOR;
		using Entry      = ENTRY;

	private:

		static constexpr size_t PAGE_SIZE_LOG2 = Descriptor::PAGE_SIZE_LOG2;
		static constexpr size_t MAX_ENTRIES    = 512;
		static constexpr size_t PAGE_SIZE      = 1UL << PAGE_SIZE_LOG2;
		static constexpr size_t PAGE_MASK      = ~((1UL << PAGE_SIZE_LOG2) - 1);

		class Misaligned {};
		class Invalid_range {};
		class Double_insertion {};

		typename Descriptor::access_t _entries[MAX_ENTRIES];

		template <typename ALLOCATOR>
		struct Insert_func
		{
			Page_flags const & flags;
			ALLOCATOR        & alloc;
			bool               flush;
			uint32_t           supported_sizes;

			Insert_func(Page_flags const & flags, ALLOCATOR & alloc, bool flush,
			            uint32_t supported_sizes)
			: flags(flags), alloc(alloc), flush(flush),
			  supported_sizes(supported_sizes)
			{ }

			void operator () (addr_t const vo, addr_t const pa,
			                  size_t const size,
			                  typename Descriptor::access_t &desc)
			{
				using Td = Descriptor::Table;
				using access_t = typename Descriptor::access_t;

				/* can we insert a large page mapping? */
				if ((supported_sizes & PAGE_SIZE) &&
				    !((vo & ~PAGE_MASK) || (pa & ~PAGE_MASK) || size < PAGE_SIZE))
				{
					access_t table_entry = Descriptor::Page::create(flags, pa);

					if (Descriptor::present(desc) &&
					    Descriptor::clear_mmu_flags(desc) != table_entry) {
						throw Double_insertion(); }

					desc = table_entry;
					if (flush)
						clflush(&desc);
					return;
				}

				/* we need to use a next level table */
				if (!Descriptor::present(desc)) {

					/* create and link next level table */
					addr_t table_phys = alloc.template construct<ENTRY>();
					desc = (access_t) Td::create(table_phys);

					if (flush)
						clflush(&desc);

				} else if (Descriptor::maps_page(desc)) {
					throw Double_insertion();
				}

				/* insert translation */
				alloc.template with_table<ENTRY>(Td::Pa::masked(desc),
					[&] (ENTRY & table) {
						table.insert_translation(vo - (vo & PAGE_MASK), pa, size,
						                         flags, alloc, flush, supported_sizes);
					},
					[&] () {
						error("Unable to get mapped table address for ",
						      Genode::Hex(Td::Pa::masked(desc)));
					});
			}
		};

		template <typename ALLOCATOR>
		struct Remove_func
		{
			ALLOCATOR & alloc;
			bool        flush;

			Remove_func(ALLOCATOR & alloc, bool flush)
			: alloc(alloc), flush(flush) { }

			void operator () (addr_t const vo, addr_t /* pa */,
			                  size_t const size,
			                  typename Descriptor::access_t &desc)
			{
				if (Descriptor::present(desc)) {
					if (Descriptor::maps_page(desc)) {
						desc = 0;
					} else {
						using Td = Descriptor::Table;

						/* use allocator to retrieve virt address of table */
						addr_t  table_phys = Td::Pa::masked(desc);

						alloc.template with_table<ENTRY>(table_phys,
							[&] (ENTRY & table) {
								addr_t const table_vo = vo - (vo & PAGE_MASK);
								table.remove_translation(table_vo, size, alloc, flush);
								if (table.empty()) {
									alloc.template destruct<ENTRY>(table_phys);
									desc = 0;
								}
							},
							[&] () {
								error("Unable to get mapped table address for ",
								      Genode::Hex(table_phys));
							});
					}

					if (desc == 0 && flush)
						clflush(&desc);
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
				size_t sz  = Genode::min(size, end-vo);

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
			if (!aligned<addr_t>((addr_t)this, ALIGNM_LOG2)) throw Misaligned();
			Genode::memset(&_entries, 0, sizeof(_entries));
		}

		/**
		 * Returns True if table does not contain any page mappings.
		 *
		 * \return   false if an entry is present, True otherwise
		 */
		bool empty()
		{
			for (unsigned i = 0; i < MAX_ENTRIES; i++)
				if (Descriptor::present(_entries[i]))
					return false;
			return true;
		}

		template <typename FN>
		void for_each_entry(FN && fn)
		{
			for (unsigned long i = 0; i < MAX_ENTRIES; i++)
				if (Descriptor::present(_entries[i]))
					fn(i, _entries[i]);
		}

		/**
		 * Insert translations into this table
		 *
		 * \param vo               offset of the virtual region represented
		 *                         by the translation within the virtual
		 *                         region represented by this table
		 * \param pa               base of the physical backing store
		 * \param size             size of the translated region
		 * \param flags            mapping flags
		 * \param alloc            second level translation table allocator
		 * \param flush            flush cache lines of table entries
		 * \param supported_sizes  supported page sizes
		 */
		template <typename ALLOCATOR>
		void insert_translation(addr_t vo, addr_t pa, size_t size,
		                        Page_flags const & flags, ALLOCATOR & alloc,
		                        bool flush = false,
					uint32_t supported_sizes = (1U << 30 | 1U << 21 | 1U << 12)) {
			_range_op(vo, pa, size,
			          Insert_func(flags, alloc, flush, supported_sizes)); }

		/**
		 * Remove translations that overlap with a given virtual region
		 *
		 * \param vo    region offset within the tables virtual region
		 * \param size  region size
		 * \param alloc second level translation table allocator
		 */
		template <typename ALLOCATOR>
		void remove_translation(addr_t vo, size_t size, ALLOCATOR & alloc,
		                        bool flush) {
			_range_op(vo, 0, size, Remove_func(alloc, flush)); }
};


template <typename ENTRY, typename DESCRIPTOR>
class Genode::Pml4_table
{
	public:

		using Descriptor = DESCRIPTOR;
		using Entry      = ENTRY;

	private:

		static constexpr size_t PAGE_SIZE_LOG2 = Descriptor::PAGE_SIZE_LOG2;
		static constexpr size_t SIZE_LOG2      = Descriptor::SIZE_LOG2;
		static constexpr size_t SIZE_MASK      = (1UL << SIZE_LOG2) - 1;
		static constexpr size_t MAX_ENTRIES    = 512;
		static constexpr size_t PAGE_SIZE      = 1UL << PAGE_SIZE_LOG2;
		static constexpr size_t PAGE_MASK      = ~((1UL << PAGE_SIZE_LOG2) - 1);

		class Misaligned {};
		class Invalid_range {};

		typename Descriptor::access_t _entries[MAX_ENTRIES];

		template <typename ALLOCATOR>
		struct Insert_func
		{
			Page_flags const & flags;
			ALLOCATOR        & alloc;
			bool               flush;
			uint32_t           supported_sizes;

			Insert_func(Page_flags const & flags,
			            ALLOCATOR & alloc, bool flush, uint32_t supported_sizes)
			: flags(flags), alloc(alloc), flush(flush),
			  supported_sizes(supported_sizes) { }

			void operator () (addr_t const vo, addr_t const pa,
			                  size_t const size,
			                  Descriptor::access_t &desc)
			{
				/* we need to use a next level table */
				if (!Descriptor::present(desc)) {
					/* create and link next level table */
					addr_t table_phys = alloc.template construct<ENTRY>();
					desc = Descriptor::create(table_phys);

					if (flush)
						clflush(&desc);
				}

				/* insert translation */
				addr_t table_phys = Descriptor::Pa::masked(desc);
				alloc.template with_table<ENTRY>(table_phys,
					[&] (ENTRY & table) {
						addr_t const table_vo = vo - (vo & PAGE_MASK);
						table.insert_translation(table_vo, pa, size, flags, alloc,
						                         flush, supported_sizes);
					},
					[&] () {
						error("Unable to get mapped table address for ",
						      Genode::Hex(table_phys));
					});
			}
		};

		template <typename ALLOCATOR>
		struct Remove_func
		{
			ALLOCATOR & alloc;
			bool        flush;

			Remove_func(ALLOCATOR & alloc, bool flush)
			: alloc(alloc), flush(flush) { }

			void operator () (addr_t const vo, addr_t /* pa */,
			                  size_t const size,
			                  Descriptor::access_t &desc)
			{
				if (Descriptor::present(desc)) {
					/* use allocator to retrieve virt address of table */
					addr_t  table_phys = Descriptor::Pa::masked(desc);
					alloc.template with_table<ENTRY>(table_phys,
						[&] (ENTRY & table) {
							addr_t const table_vo = vo - (vo & PAGE_MASK);
							table.remove_translation(table_vo, size, alloc, flush);
							if (table.empty()) {
								alloc.template destruct<ENTRY>(table_phys);
								desc = 0;

								if (flush)
									clflush(&desc);
							}
						},
						[&] () {
							error("Unable to get mapped table address for ",
							      Genode::Hex(table_phys));
						});
				}
			}
		};

		template <typename FUNC>
		void _range_op(addr_t vo, addr_t pa, size_t size, FUNC &&func)
		{
			for (size_t i = (vo & SIZE_MASK) >> PAGE_SIZE_LOG2; size > 0;
			     i = (vo & SIZE_MASK) >> PAGE_SIZE_LOG2) {
				addr_t end = (vo + PAGE_SIZE) & PAGE_MASK;
				size_t sz  = Genode::min(size, end-vo);

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
		static constexpr size_t _count(size_t region, size_t alignment)
		{
			return Genode::align_addr<size_t>(region, (int)alignment)
			       / (1UL << alignment);
		}

	public:

		static constexpr size_t MIN_PAGE_SIZE_LOG2 = SIZE_LOG2_4KB;
		static constexpr size_t ALIGNM_LOG2        = SIZE_LOG2_4KB;

		Pml4_table()
		{
			if (!aligned<addr_t>((addr_t)this, ALIGNM_LOG2)) throw Misaligned();
			Genode::memset(&_entries, 0, sizeof(_entries));
		}

		explicit Pml4_table(Pml4_table & kernel_table) : Pml4_table()
		{
			static size_t first = (0xffffffc000000000 & SIZE_MASK) >> PAGE_SIZE_LOG2;
			for (size_t i = first; i < MAX_ENTRIES; i++)
				_entries[i] = kernel_table._entries[i];
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

		template <typename FN>
		void for_each_entry(FN && fn)
		{
			for (unsigned long i = 0; i < MAX_ENTRIES; i++) {
				if (Descriptor::present(_entries[i]))
					fn(i, _entries[i]);
			}
		}

		/**
		 * Insert translations into this table
		 *
		 * \param vo               offset of the virtual region represented
		 *                         by the translation within the virtual
		 *                         region represented by this table
		 * \param pa               base of the physical backing store
		 * \param size             size of the translated region
		 * \param flags            mapping flags
		 * \param alloc            second level translation table allocator
		 * \param flush            flush cache lines of table entries
		 * \param supported_sizes  supported page sizes
		 */
		template <typename ALLOCATOR>
		void insert_translation(addr_t vo, addr_t pa, size_t size,
		                        Page_flags const & flags, ALLOCATOR & alloc,
		                        bool flush = false, uint32_t supported_sizes = (1U << 30 | 1U << 21 | 1U << 12)) {
			_range_op(vo, pa, size,
			          Insert_func(flags, alloc, flush, supported_sizes)); }

		/**
		 * Remove translations that overlap with a given virtual region
		 *
		 * \param vo    region offset within the tables virtual region
		 * \param size  region size
		 * \param alloc second level translation table allocator
		 */
		template <typename ALLOCATOR>
		void remove_translation(addr_t vo, size_t size, ALLOCATOR & alloc,
		                        bool flush = false)
		{
			_range_op(vo, 0, size, Remove_func(alloc, flush));
		}

} __attribute__((aligned(1 << ALIGNM_LOG2)));

#endif /* _INCLUDE__SPEC__X86_64__PAGE_TABLE__PAGE_TABLE_BASE_H_ */
