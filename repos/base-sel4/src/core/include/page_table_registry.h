/*
 * \brief   Associate page-table and frame selectors with virtual addresses
 * \author  Norman Feske
 * \date    2015-05-04
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__PAGE_TABLE_REGISTRY_H_
#define _CORE__INCLUDE__PAGE_TABLE_REGISTRY_H_

/* Genode includes */
#include <util/list.h>
#include <base/exception.h>
#include <base/tslab.h>

/* core includes */
#include <util.h>

namespace Genode { class Page_table_registry; }


class Genode::Page_table_registry
{
	public:

		class Lookup_failed : Exception { };

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

				Page_table(addr_t addr) : addr(addr) { }

				void insert_entry(Allocator &entry_slab, addr_t addr, unsigned sel)
				{
					if (_entry_exists(addr)) {
						PWRN("trying to insert page frame for 0x%lx twice", addr);
						return;
					}

					_entries.insert(new (entry_slab) Entry(addr, sel));
				}
		};

		class Slab_block : public Genode::Slab_block { long _data[4*1024]; };

		template <typename T>
		struct Slab : Genode::Tslab<T, sizeof(Slab_block)>
		{
			Slab_block _initial_block;

			Slab(Allocator &md_alloc)
			:
				Tslab<T, sizeof(Slab_block)>(&md_alloc, &_initial_block)
			{ }
		};

		Slab<Page_table>        _page_table_slab;
		Slab<Page_table::Entry> _page_table_entry_slab;

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
			PDBG("page-table lookup failed");
			throw Lookup_failed();
		}

	public:

		/**
		 * Constructor
		 *
		 * \param md_alloc  backing store allocator for metadata
		 */
		Page_table_registry(Allocator &md_alloc)
		:
			_page_table_slab(md_alloc),
			_page_table_entry_slab(md_alloc)
		{ }

		/**
		 * Register page table
		 *
		 * \param addr  virtual address
		 * \param sel   page-table selector
		 */
		void insert_page_table(addr_t addr, unsigned sel)
		{
			if (_page_table_exists(addr)) {
				PWRN("trying to insert page table for 0x%lx twice", addr);
				return;
			}

			_page_tables.insert(new (_page_table_slab) Page_table(addr));
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
			_lookup(addr).insert_entry(_page_table_entry_slab, addr, sel);
		}
};

#endif /* _CORE__INCLUDE__PAGE_TABLE_REGISTRY_H_ */
