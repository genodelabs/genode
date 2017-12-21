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
#include <base/exception.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/tslab.h>
#include <util/avl_tree.h>

/* core includes */
#include <util.h>
#include <cap_sel_alloc.h>

namespace Genode { class Page_table_registry; }

class Genode::Page_table_registry
{
	public:

		class Mapping_cache_full : Exception { };

	private:

		enum Level { FRAME, PAGE_TABLE, LEVEL2, LEVEL3 };

		class Frame : public Avl_node<Frame>
		{
			private:

				addr_t  const _vaddr;
				Cap_sel const _sel;

				Frame *_lookup(addr_t vaddr)
				{
					if (vaddr == _vaddr) return this;

					Frame *e = Avl_node<Frame>::child(vaddr > _vaddr);

					return e ? e->_lookup(vaddr) : 0;
				}

				static addr_t _base(addr_t const vaddr, unsigned const log2base)
				{
					addr_t const size = 1UL << log2base;
					return vaddr & ~(size - 1);
				}

			public:

				Frame(addr_t const vaddr, Cap_sel const sel, unsigned log2base)
				:
					_vaddr(_base(vaddr, log2base)), _sel(sel)
				{ }

				Cap_sel   sel() const { return _sel; }
				addr_t  vaddr() const { return _vaddr; }

				static Frame * lookup(Avl_tree<Frame> &tree,
				                      addr_t const vaddr,
				                      unsigned const log2base)
				{
					Frame * element = tree.first();
					if (!element)
						return nullptr;

					addr_t const align_addr = _base(vaddr, log2base);
					return element->_lookup(align_addr);
				}

				bool higher(Frame const *other) const {
					return other->_vaddr > _vaddr; }
		};

		class Table : public Avl_node<Table>
		{
			private:

				addr_t  const _vaddr;
				addr_t  const _paddr;
				Cap_sel const _sel;

				Table *_lookup(addr_t vaddr)
				{
					if (vaddr == _vaddr) return this;

					Table *e = Avl_node<Table>::child(vaddr > _vaddr);

					return e ? e->_lookup(vaddr) : 0;
				}

				static addr_t _base(addr_t const vaddr, unsigned const log2base)
				{
					addr_t const size = 1UL << log2base;
					return vaddr & ~(size - 1);
				}

			public:

				Table(addr_t const vaddr, addr_t const paddr,
				     Cap_sel const sel, unsigned log2base)
				:
					_vaddr(_base(vaddr, log2base)), _paddr(paddr), _sel(sel)
				{ }

				Cap_sel sel()   const { return _sel; }
				addr_t  vaddr() const { return _vaddr; }
				addr_t  paddr() const { return _paddr; }

				static Table * lookup(Avl_tree<Table> &tree,
				                     addr_t const vaddr,
				                     unsigned const log2base)
				{
					Table * element = tree.first();
					if (!element)
						return nullptr;

					addr_t const align_addr = _base(vaddr, log2base);
					return element->_lookup(align_addr);
				}

				bool higher(Table const *other) const {
					return other->_vaddr > _vaddr; }
		};

		enum {
			LEVEL_0 = 12, /* 4K Page */
		};

		static constexpr size_t SLAB_BLOCK_SIZE = get_page_size() - Sliced_heap::meta_data_size();
		Tslab<Frame, SLAB_BLOCK_SIZE> _alloc_frames;
		uint8_t _initial_sb_frame[SLAB_BLOCK_SIZE];

		Tslab<Table, SLAB_BLOCK_SIZE> _alloc_high;
		uint8_t _initial_sb_high[SLAB_BLOCK_SIZE];

		Avl_tree<Frame> _frames { };
		Avl_tree<Table> _level1 { };
		Avl_tree<Table> _level2 { };
		Avl_tree<Table> _level3 { };

		void _insert(addr_t const vaddr, Cap_sel const sel, Level const level,
		             addr_t const paddr, unsigned const level_log2_size)
		{
			try {
				switch (level) {
				case FRAME:
					_frames.insert(new (_alloc_frames) Frame(vaddr, sel,
					                                         level_log2_size));
					break;
				case PAGE_TABLE:
					_level1.insert(new (_alloc_high) Table(vaddr, paddr, sel,
					                                         level_log2_size));
					break;
				case LEVEL2:
					_level2.insert(new (_alloc_high) Table(vaddr, paddr, sel,
					                                      level_log2_size));
					break;
				case LEVEL3:
					_level3.insert(new (_alloc_high) Table(vaddr, paddr, sel,
					                                      level_log2_size));
					break;
				}
			} catch (Genode::Allocator::Out_of_memory) {
				throw Mapping_cache_full();
			} catch (Genode::Out_of_caps) {
				throw Mapping_cache_full();
			}
		}

		template <typename FN, typename T>
		void _flush_high(FN const &fn, Avl_tree<T> &tree, Allocator &alloc)
		{
			for (T *element; (element = tree.first());) {

				fn(element->sel(), element->paddr());

				tree.remove(element);
				destroy(alloc, element);
			}
		}

	public:

		/**
		 * Constructor
		 *
		 * \param md_alloc  backing store allocator for metadata
		 */
		Page_table_registry(Allocator &md_alloc)
		:
			_alloc_frames(md_alloc, _initial_sb_frame),
			_alloc_high(md_alloc, _initial_sb_high)
		{ }

		~Page_table_registry()
		{
			if (_frames.first() || _level1.first() || _level2.first() ||
			    _level3.first())
				error("still entries in page table registry in destruction");
		}

		bool page_frame_at(addr_t const vaddr) {
			return Frame::lookup(_frames, vaddr, LEVEL_0); }
		bool page_table_at(addr_t const vaddr, addr_t const level_log2) {
			return Table::lookup(_level1, vaddr, level_log2); }
		bool page_directory_at(addr_t const vaddr, addr_t const level_log2) {
			return Table::lookup(_level2, vaddr, level_log2); }
		bool page_level3_at(addr_t const vaddr, addr_t const level_log2) {
			return Table::lookup(_level3, vaddr, level_log2); }

		void insert_page_frame(addr_t const vaddr, Cap_sel const sel) {
			_insert(vaddr, sel, Level::FRAME, 0, LEVEL_0); }
		void insert_page_table(addr_t const vaddr, Cap_sel const sel,
		                       addr_t const paddr, addr_t const level_log2) {
			_insert(vaddr, sel, Level::PAGE_TABLE, paddr, level_log2); }
		void insert_page_directory(addr_t const vaddr, Cap_sel const sel,
		                           addr_t const paddr, addr_t const level_log2) {
			_insert(vaddr, sel, Level::LEVEL2, paddr, level_log2); }
		void insert_page_level3(addr_t const vaddr, Cap_sel const sel,
		                        addr_t const paddr, addr_t const level_log2) {
			_insert(vaddr, sel, Level::LEVEL3, paddr, level_log2); }

		/**
		 * Apply functor 'fn' to selector of specified virtual address and
		 * flush the page frame from the this cache.
		 *
		 * \param vaddr  virtual address
		 *
		 * The functor is called with the selector of the page table entry
		 * (the copy of the phys frame selector) as argument.
		 */
		template <typename FN>
		void flush_page(addr_t vaddr, FN const &fn)
		{
			Frame * frame = Frame::lookup(_frames, vaddr, LEVEL_0);
			if (!frame)
				return;

			fn(frame->sel(), frame->vaddr());
			_frames.remove(frame);
			destroy(_alloc_frames, frame);
		}

		template <typename FN>
		void flush_pages(FN const &fn)
		{
			Avl_tree<Frame> tmp;

			for (Frame *frame; (frame = _frames.first());) {

				if (fn(frame->sel(), frame->vaddr())) {
					_frames.remove(frame);
					destroy(_alloc_frames, frame);
				} else {
					_frames.remove(frame);
					tmp.insert(frame);
				}
			}

			for (Frame *frame; (frame = tmp.first());) {
				tmp.remove(frame);
				_frames.insert(frame);
			}
		}

		template <typename PG, typename LV>
		void flush_all(PG const &pages, LV const &level)
		{
			flush_pages(pages);
			_flush_high(level, _level1, _alloc_high);
			_flush_high(level, _level2, _alloc_high);
			_flush_high(level, _level3, _alloc_high);
		}
};

#endif /* _CORE__INCLUDE__PAGE_TABLE_REGISTRY_H_ */
