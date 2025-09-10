/*
 * \brief   Dynamic page table allocator
 * \author  Stefan Kalkowski
 * \date    2025-08-05
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__PAGE_TABLE_ALLOCATOR_H_
#define _CORE__PAGE_TABLE_ALLOCATOR_H_

/* base includes */
#include <base/heap.h>
#include <util/dictionary.h>

/* base-hw internal includes */
#include <hw/page_table_allocator.h>
#include <phys_allocated.h>

namespace Core { class Page_table_allocator; }

/**
 * Dynamic page-table allocator for all PDs except core,
 * which uses accountable allocators of the PD session components
 */
class Core::Page_table_allocator : public Hw::Page_table_allocator
{
	private:

		struct Entry;
		struct Key;

		using Dictionary = Genode::Dictionary<Key, addr_t>;

		struct Key : Dictionary::Element
		{
			Entry &entry;

			Key(Dictionary &dict, addr_t addr, Entry &entry)
				: Dictionary::Element(dict, addr), entry(entry) {}
		};

		struct Entry;
		using List_element = Genode::List_element<Entry>;
		using List         = Genode::List<List_element>;

		struct Entry
		{
			struct Table { uint8_t _[get_page_size()]; };
			Phys_allocated<Table> table;

			addr_t virt()
			{
				addr_t ret = 0;
				table.obj([&] (Table &t) { ret = (addr_t)&t; });
				return ret;
			}

			Key v;
			Key p;

			List_element elem { this };

			Entry(Accounted_mapped_ram_allocator &ram,
			      Dictionary                     &virt_dict,
			      Dictionary                     &phys_dict)
			:
				table(ram),
				v(virt_dict, virt(), *this),
				p(phys_dict, table.phys_addr(), *this) {}
		};

		Accounted_mapped_ram_allocator &_accounted_mapped_ram;

		static constexpr size_t SLAB_BLOCK_SIZE =
			get_page_size() - Sliced_heap::meta_data_size();
		uint8_t _initial_sb_tables[SLAB_BLOCK_SIZE];
		Tslab<Entry, SLAB_BLOCK_SIZE> _alloc_tables;

		List _empty_list {};

		Dictionary _virt_dict {};
		Dictionary _phys_dict {};

	public:

		Page_table_allocator(Accounted_mapped_ram_allocator &ram,
		                     Allocator &heap)
		:
			_accounted_mapped_ram(ram),
			_alloc_tables(heap, _initial_sb_tables)
		{}

		~Page_table_allocator()
		{
			while (_empty_list.first()) {
				List_element *le = _empty_list.first();
				_empty_list.remove(le);
				le->object()->~Entry();
				Allocation { _alloc_tables, { le->object(), sizeof(Entry) } };
			}
		}

		Attempt<addr_t, Lookup_error> _phys_addr(addr_t virt_addr) override
		{
			using Result = Attempt<addr_t, Lookup_error>;
			return _virt_dict.with_element(virt_addr,
				[] (Key &k) -> Result { return k.entry.table.phys_addr(); },
				[] ()       -> Result { return Lookup_error(); });
		}

		Attempt<addr_t, Lookup_error> _virt_addr(addr_t phys_addr) override
		{
			using Result = Attempt<addr_t, Lookup_error>;
			return _phys_dict.with_element(phys_addr,
				[] (Key &k) -> Result { return k.entry.virt(); },
				[] ()       -> Result { return Lookup_error(); });
		}

		Allocation::Attempt try_alloc(size_t) override
		{
			using Error = Accounted_mapped_ram_allocator::Error;

			void *ptr = nullptr;
			List_element *le = _empty_list.first();

			if (le) {
				_empty_list.remove(le);
				Entry &entry = *le->object();
				entry.table.obj([&] (Entry::Table &t) { ptr = &t; });
				return { *this, { ptr, sizeof(Entry::Table) }};
			}

			return _alloc_tables.try_alloc(sizeof(Entry)).convert<Allocation::Attempt>(
				[&] (auto &res) {
					Entry &entry =
						*construct_at<Entry>(res.ptr, _accounted_mapped_ram,
					                         _virt_dict, _phys_dict);
					return entry.table.constructed.convert<Allocation::Attempt>(
						[&] (Ok) -> Allocation::Attempt {
							entry.table.obj([&] (Entry::Table &t) { ptr = &t; });
							res.deallocate = false;
							return { *this, { ptr, sizeof(Entry::Table) }};
						},
						[&] (Error e) -> Allocation::Attempt {
							entry.~Entry();
							switch(e) {
							case Error::OUT_OF_RAM: return Alloc_error::OUT_OF_RAM;
							case Error::DENIED:     break;
							};
							return Alloc_error::DENIED;
						});
				},
				[] (Alloc_error e) { return e; });
		}

		void _free(Allocation &a) override
		{
			_virt_dict.with_element((addr_t)a.ptr,
				[&] (Key &k) { _empty_list.insert(&k.entry.elem); },
				[] () { });
		}
};

#endif /* _CORE__PAGE_TABLE_ALLOCATOR_H_ */
