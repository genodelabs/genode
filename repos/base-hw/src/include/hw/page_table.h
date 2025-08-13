/*
 * \brief  Generic page table tool
 * \author Stefan Kalkowski
 * \date   2025-06-17
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__INCLUDE__HW__PAGE_TABLE_H_
#define _SRC__INCLUDE__HW__PAGE_TABLE_H_

#include <hw/page_table_allocator.h>

#include <util/attempt.h>
#include <util/misc_math.h>
#include <util/register.h>
#include <util/string.h>

namespace Hw {

	using namespace Genode;

	enum {
		SIZE_LOG2_1KB   = 10,
		SIZE_LOG2_4KB   = 12,
		SIZE_LOG2_16KB  = 14,
		SIZE_LOG2_1MB   = 20,
		SIZE_LOG2_2MB   = 21,
		SIZE_LOG2_1GB   = 30,
		SIZE_LOG2_4GB   = 32,
		SIZE_LOG2_256GB = 38,
		SIZE_LOG2_512GB = 39,
		SIZE_LOG2_256TB = 48,
	};

	using Page_table_insertion_result = Attempt<Ok, Page_table_error>;

	enum class Page_table_entry { INVALID, TABLE, BLOCK };

	template <typename, size_t, size_t> class Page_table_tpl;
	template <typename, size_t, size_t> class Page_table_leaf;
	template <typename, typename, size_t, size_t> class Page_table_node;
}


template <typename DESCRIPTOR,
          Genode::size_t PAGE_SIZE_LOG2,
          Genode::size_t SIZE_LOG2>
class Hw::Page_table_tpl
{
	public:

		static constexpr size_t PAGE_SIZE   = 1UL << PAGE_SIZE_LOG2;
		static constexpr size_t MAX_ENTRIES = 1UL << (SIZE_LOG2-PAGE_SIZE_LOG2);

		using Result = Page_table_insertion_result;

	protected:

		typename DESCRIPTOR::access_t _entries[MAX_ENTRIES];

		static constexpr addr_t _page_mask_low(addr_t a) {
			return a & ((1UL << PAGE_SIZE_LOG2) - 1); }

		static constexpr addr_t _page_mask_high(addr_t a) {
			return a & ~((1UL << PAGE_SIZE_LOG2) - 1); }

		static constexpr size_t _idx(addr_t virt_addr) {
			return (virt_addr >> PAGE_SIZE_LOG2) & (MAX_ENTRIES-1); };

		static constexpr bool _aligned_and_fits(addr_t vaddr, addr_t paddr,
		                                        size_t size)
		{
			return !_page_mask_low(vaddr) &&
			       !_page_mask_low(paddr) &&
			       size >= PAGE_SIZE;
		}

		Result _for_range(addr_t virt_addr, addr_t phys_addr,
		                  size_t size, auto const &fn)
		{
			Result result = Ok();

			for (size_t i = _idx(virt_addr); size > 0; i = _idx(virt_addr)) {
				addr_t end = _page_mask_high(virt_addr + PAGE_SIZE);
				size_t sz  = min(size, end - virt_addr);

				result = fn(virt_addr, phys_addr, sz, _entries[i]);
				if (result.failed())
					return result;

				/* check whether we wrap */
				if (end < virt_addr)
					break;

				size = size - sz;
				virt_addr += sz;
				phys_addr += sz;
			}

			return result;
		}

		void _for_range(addr_t virt_addr, size_t size, auto const &fn)
		{
			for (size_t i = _idx(virt_addr); size > 0; i = _idx(virt_addr)) {
				addr_t end = _page_mask_high(virt_addr + PAGE_SIZE);
				size_t sz  = min(size, end - virt_addr);

				fn(virt_addr, sz, _entries[i]);

				/* check whether we wrap */
				if (end < virt_addr)
					break;

				size = size - sz;
				virt_addr += sz;
			}
		}

		/**
		 * Return how many entries of an alignment fit into region
		 */
		template <typename T>
		static constexpr size_t _table_count(size_t region, T align)
		{
			size_t r = (size_t)align_addr<uint64_t>(region, (int)align)
			           / (1ULL << align);
			return r ? r : 1;
		}

		/**
		 * Return how many entries of several alignments fit into region
		 */
		template <typename T, typename... Tail>
		static constexpr size_t _table_count(size_t region, T align,
		                                     Tail... tail)
		{
			return _table_count(region, align) + _table_count(region, tail...);
		}

		static constexpr size_t _core_vm_size() {
			return 1UL << SIZE_LOG2_1GB; }

	public:

		Page_table_tpl() {
			Genode::memset(&_entries, 0, sizeof(_entries)); }

		bool empty()
		{
			for (unsigned i = 0; i < MAX_ENTRIES; i++)
				if (DESCRIPTOR::present(_entries[i]))
					return false;
			return true;
		}
};


template <typename DESCRIPTOR,
          Genode::size_t PAGE_SIZE_LOG2,
          Genode::size_t SIZE_LOG2>
class Hw::Page_table_leaf
: public Hw::Page_table_tpl<DESCRIPTOR, PAGE_SIZE_LOG2, SIZE_LOG2>
{
	public:

		using Base = Page_table_tpl<DESCRIPTOR, PAGE_SIZE_LOG2, SIZE_LOG2>;
		using Result = Base::Result;
		using desc_t = typename DESCRIPTOR::access_t;

	protected:

		using Base::_for_range;
		using Base::_aligned_and_fits;
		using Base::_page_mask_high;

	public:

		template <typename ALLOCATOR>
		Result insert(addr_t vo, addr_t pa, size_t size,
		              Page_flags const &flags, ALLOCATOR &,
		              auto const &table_changed)
		{
			return _for_range(vo, pa, size,
				[&] (addr_t vo, addr_t pa, size_t size, desc_t &desc) -> Result
				{
					if (!_aligned_and_fits(vo, pa, size))
						return Page_table_error::INVALID_RANGE;

					desc_t blk_desc = DESCRIPTOR::create(flags, pa);

					if (DESCRIPTOR::conflicts(desc, blk_desc))
						return Page_table_error::INVALID_RANGE;

					desc = blk_desc;
					table_changed((addr_t)&desc, sizeof(desc));
					return Ok();
				});
		}

		template <typename ALLOCATOR>
		Result insert(addr_t vo, addr_t pa, size_t size,
		              Page_flags const &flags, ALLOCATOR &alloc) {
			return insert(vo, pa, size, flags, alloc, [] (addr_t, size_t) {}); }

		template <typename ALLOCATOR>
		void remove(addr_t vo, size_t size, ALLOCATOR&,
		            auto const &table_changed)
		{
			_for_range(vo, size, [&] (addr_t, size_t, desc_t &desc) {
				desc = 0;
				table_changed((addr_t)&desc, sizeof(desc));
			});
		}

		template <typename ALLOCATOR>
		void remove(addr_t vo, size_t size, ALLOCATOR &alloc) {
			remove(vo, size, alloc, [] (addr_t, size_t) {}); }

		template <typename ALLOCATOR>
		Result lookup(addr_t const virt, addr_t &phys,
		              ALLOCATOR &)
		{
			return _for_range(virt, 0, 1,
				[&] (addr_t, addr_t, size_t, desc_t &desc) -> Result
				{
					if (!DESCRIPTOR::present(desc))
						return Page_table_error::INVALID_RANGE;

					phys = DESCRIPTOR::address(desc);
					if (DESCRIPTOR::writeable(desc))
						return Ok();
					return Page_table_error::INVALID_RANGE;
				});
		}
};


template <typename ENTRY, typename DESCRIPTOR,
          Genode::size_t PAGE_SIZE_LOG2,
          Genode::size_t SIZE_LOG2>
class Hw::Page_table_node
: public Hw::Page_table_tpl<DESCRIPTOR, PAGE_SIZE_LOG2, SIZE_LOG2>
{
	public:

		using Base = Page_table_tpl<DESCRIPTOR, PAGE_SIZE_LOG2, SIZE_LOG2>;
		using Result = Base::Result;
		using desc_t = typename DESCRIPTOR::access_t;

	protected:

		using Base::_for_range;
		using Base::_aligned_and_fits;
		using Base::_page_mask_high;

	public:

		template <typename ALLOCATOR>
		Result insert(addr_t vo, addr_t pa, size_t size,
		              Page_flags const &flags, ALLOCATOR &alloc,
		              auto const &table_changed)
		{
			Result result = _for_range(vo, pa, size,
				[&] (addr_t vo, addr_t pa, size_t size, desc_t &desc) -> Result
				{
					/* can we insert a whole block? */
					if (_aligned_and_fits(vo, pa, size)) {
						desc_t blk_desc = DESCRIPTOR::create(flags, pa);

						if (DESCRIPTOR::conflicts(desc, blk_desc))
							return Page_table_error::INVALID_RANGE;

						desc = blk_desc;
						table_changed((addr_t)&desc, sizeof(desc));
						return Ok();
					}

					/* we need to use a next level table */
					switch (DESCRIPTOR::type(desc)) {

					case Page_table_entry::INVALID: /* no entry */
						{
							/* create and link next level table */
							Result r = alloc.template create<ENTRY,
							                                 DESCRIPTOR>(desc);
							if (r.failed())
								return r;
							table_changed((addr_t)&desc, sizeof(desc));
						}
						[[fallthrough]];

					case Page_table_entry::TABLE: /* table already available */
						{
							addr_t phys = DESCRIPTOR::address(desc);
							return alloc.template lookup<ENTRY>(phys,
							                                    [&] (ENTRY &e) {
								return e.insert(vo-_page_mask_high(vo),
								                pa, size, flags, alloc,
								                table_changed); });
						}

					case Page_table_entry::BLOCK: /* there is already a block */
						break;
					};

					return Page_table_error::INVALID_RANGE;
				});
			return result;
		}

		template <typename ALLOCATOR>
		Result insert(addr_t vo, addr_t pa, size_t size,
		              Page_flags const &flags, ALLOCATOR &alloc) {
			return insert(vo, pa, size, flags, alloc, [] (addr_t, size_t) {}); }

		template <typename ALLOCATOR>
		void remove(addr_t vo, size_t size, ALLOCATOR &alloc,
		            auto const &table_changed = [] (addr_t, size_t) {})
		{
			_for_range(vo, size,
				[&] (addr_t vo, size_t size, desc_t &desc) {

				switch (DESCRIPTOR::type(desc)) {
				case Page_table_entry::TABLE:
					/* use allocator to retrieve virt address of table */
					alloc.template lookup<ENTRY>(DESCRIPTOR::address(desc),
						[&] (ENTRY &table) {
						table.remove(vo-_page_mask_high(vo), size, alloc,
						             table_changed);
						if (table.empty()) {
							alloc.template destroy<ENTRY>(table);
							desc = 0;
							table_changed((addr_t)&desc, sizeof(desc));
						}
						return Ok();
					}).with_error([] (Page_table_error) { /* ignore */ });
					return;
				case Page_table_entry::BLOCK: [[fallthrough]];
				case Page_table_entry::INVALID:
					desc = 0;
				}
			});
		}

		template <typename ALLOCATOR>
		void remove(addr_t vo, size_t size, ALLOCATOR &alloc) {
			remove(vo, size, alloc, [] (addr_t, size_t) {}); }

		template <typename ALLOCATOR>
		Result lookup(addr_t const virt, addr_t &phys,
		              ALLOCATOR &alloc)
		{
			return _for_range(virt, 0, 1,
				[&] (addr_t vo, addr_t, size_t, desc_t &desc) -> Result
				{
					switch (DESCRIPTOR::type(desc)) {
					case Page_table_entry::BLOCK:
						phys = DESCRIPTOR::address(desc);
						if (DESCRIPTOR::writeable(desc))
							return Ok();
						return Page_table_error::INVALID_RANGE;
					case Page_table_entry::TABLE:
						return alloc.template lookup<ENTRY>(DESCRIPTOR::address(desc),
							[&] (ENTRY &table) {
							return table.lookup(vo-_page_mask_high(vo), phys,
							                    alloc);
						});
					case Page_table_entry::INVALID: break;
					};
					return Page_table_error::INVALID_RANGE;
				});
		}
};

#endif /* _SRC__INCLUDE__HW__PAGE_TABLE_H_ */
