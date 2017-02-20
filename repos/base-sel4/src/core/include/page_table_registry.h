/*
 * \brief   Associate page-table and frame selectors with virtual addresses
 * \author  Norman Feske
 * \date    2015-05-04
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__PAGE_TABLE_REGISTRY_H_
#define _CORE__INCLUDE__PAGE_TABLE_REGISTRY_H_

/* Genode includes */
#include <util/list.h>
#include <base/exception.h>
#include <base/log.h>

/* core includes */
#include <util.h>
#include <cap_sel_alloc.h>

namespace Genode { class Page_table_registry; }


class Genode::Page_table_registry
{
	public:

		class Lookup_failed : Exception { };
		class Mapping_cache_full : Exception { };

	private:

		/*
		 * XXX use AVL tree (with virtual address as key) instead of list
		 */

		class Page_table : public List<Page_table>::Element
		{
			public:

				struct Entry : List<Entry>::Element
				{
					addr_t   const addr;
					unsigned const sel;

					Entry(addr_t addr, unsigned sel) : addr(addr), sel(sel) { }
				};

				addr_t const addr;

			private:

				List<Entry> _entries;

				static addr_t _page_frame_base(addr_t addr)
				{
					return addr & get_page_mask();
				}

				bool _entry_exists(addr_t addr) const
				{
					for (Entry const *e = _entries.first(); e; e = e->next()) {
						if (_page_frame_base(e->addr) == _page_frame_base(addr))
							return true;
					}
					return false;
				}

			public:

				class Lookup_failed : Exception { };

				Page_table(addr_t addr) : addr(addr) { }

				Entry *first() { return _entries.first(); }

				Entry &lookup(addr_t addr)
				{
					for (Entry *e = _entries.first(); e; e = e->next()) {
						if (_page_frame_base(e->addr) == _page_frame_base(addr))
							return *e;
					}
					throw Lookup_failed();
				}

				void insert_entry(Allocator &entry_alloc, addr_t addr, unsigned sel)
				{
					if (_entry_exists(addr)) {
						warning("trying to insert page frame for ", Hex(addr), " twice");
						return;
					}

					try {
						_entries.insert(new (entry_alloc) Entry(addr, sel));
					} catch (Genode::Allocator::Out_of_memory) {
						throw Mapping_cache_full();
					}
				}

				void remove_entry(Allocator &entry_alloc, addr_t addr)
				{
					try {
						Entry &entry = lookup(addr);
						_entries.remove(&entry);
						destroy(entry_alloc, &entry);
					} catch (Lookup_failed) { }
				}

				void flush_all(Allocator &entry_alloc)
				{
					for (; Entry *entry = _entries.first();) {
						_entries.remove(entry);
						destroy(entry_alloc, entry);
					}
				}
		};

		/**
		 * Allocator operating on a static memory pool
		 *
		 * \param ELEM  element type
		 * \param MAX   maximum number of elements
		 *
		 * The size of a single ELEM must be a multiple of sizeof(long).
		 */
		template <typename ELEM, size_t MAX>
		class Static_allocator : public Allocator
		{
			private:

				Bit_allocator<MAX> _used;

				struct Elem_space
				{
					long space[sizeof(ELEM)/sizeof(long)];
				};

				Elem_space _elements[MAX];

			public:

				class Alloc_failed { };

				bool alloc(size_t size, void **out_addr) override
				{
					*out_addr = nullptr;

					if (size > sizeof(Elem_space)) {
						error("unexpected allocation size of ", size);
						return false;
					}

					try {
						*out_addr = &_elements[_used.alloc()]; }
					catch (typename Bit_allocator<MAX>::Out_of_indices) {
						return false; }

					return true;
				}

				size_t overhead(size_t) const override { return 0; }

				void free(void *ptr, size_t) override
				{
					Elem_space *elem = reinterpret_cast<Elem_space *>(ptr);
					unsigned const index = elem - &_elements[0];
					_used.free(index);
				}

				bool need_size_for_free() const { return false; }
		};

		Static_allocator<Page_table, 128>         _page_table_alloc;
		Static_allocator<Page_table::Entry, 3 * 1024> _page_table_entry_alloc;

		List<Page_table> _page_tables;

		static addr_t _page_table_base(addr_t addr)
		{
			return addr & ~(4*1024*1024 - 1);
		}

		bool _page_table_exists(addr_t addr) const
		{
			for (Page_table const *pt = _page_tables.first(); pt; pt = pt->next()) {
				if (_page_table_base(pt->addr) == _page_table_base(addr))
					return true;
			}
			return false;
		}

		Page_table &_lookup(addr_t addr)
		{
			for (Page_table *pt = _page_tables.first(); pt; pt = pt->next()) {
				if (_page_table_base(pt->addr) == _page_table_base(addr))
					return *pt;
			}
			warning(__func__, ": page-table lookup failed");
			throw Lookup_failed();
		}

	public:

		/**
		 * Constructor
		 *
		 * \param md_alloc  backing store allocator for metadata
		 *
		 * XXX The md_alloc argument is currently unused as we dimension
		 *     MAX_PAGE_TABLES and MAX_PAGE_TABLE_ENTRIES statically.
		 */
		Page_table_registry(Allocator &md_alloc) { }

		~Page_table_registry()
		{
			if (_page_tables.first())
				error("still entries in page table registry in destruction");
		}

		/**
		 * Register page table
		 *
		 * \param addr  virtual address
		 * \param sel   page-table selector
		 */
		void insert_page_table(addr_t addr, Cap_sel sel)
		{
			/* XXX sel is unused */

			if (_page_table_exists(addr)) {
				warning("attempt to insert page table for ", Hex(addr), " twice");
				return;
			}

			_page_tables.insert(new (_page_table_alloc) Page_table(addr));
		}

		bool has_page_table_at(addr_t addr) const
		{
			return _page_table_exists(addr);
		}

		/**
		 * Register page table entry
		 *
		 * \param addr  virtual address
		 * \param sel   page frame selector
		 *
		 * \throw  Lookup_failed  no page table for given address
		 */
		void insert_page_table_entry(addr_t addr, unsigned sel)
		{
			_lookup(addr).insert_entry(_page_table_entry_alloc, addr, sel);
		}

		/**
		 * Discard the information about the given virtual address
		 */
		void forget_page_table_entry(addr_t addr)
		{
			try {
				Page_table &page_table = _lookup(addr);
				page_table.remove_entry(_page_table_entry_alloc, addr);
			} catch (...) { }
		}


		void flush_cache()
		{
			for (Page_table *pt = _page_tables.first(); pt; pt = pt->next())
				pt->flush_all(_page_table_entry_alloc);
		}

		/**
		 * Apply functor 'fn' to selector of specified virtual address
		 *
		 * \param addr  virtual address
		 *
		 * The functor is called with the selector of the page table entry
		 * (the copy of the phys frame selector) as argument.
		 */
		template <typename FN>
		void apply(addr_t addr, FN const &fn)
		{
			try {
				Page_table        &page_table = _lookup(addr);
				Page_table::Entry &entry      = page_table.lookup(addr);

				fn(entry.sel);
			} catch (...) { }
		}

		template <typename FN>
		void apply_to_and_destruct_all(FN const &fn)
		{
			for (Page_table *pt; (pt = _page_tables.first());) {

				Page_table::Entry *entry = pt->first();
				for (; entry; entry = entry->next())
					fn(entry->sel);

				pt->flush_all(_page_table_entry_alloc);

				_page_tables.remove(pt);
				destroy(_page_table_alloc, pt);
			}
		}
};

#endif /* _CORE__INCLUDE__PAGE_TABLE_REGISTRY_H_ */
