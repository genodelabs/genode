/*
 * \brief  Slab allocator with aligned slab entries
 * \author Stefan Kalkowski
 * \date   2014-03-04
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__PAGE_SLAB_H_
#define _CORE__INCLUDE__PAGE_SLAB_H_

#include <base/allocator.h>
#include <base/stdint.h>
#include <util/list.h>
#include <util/bit_allocator.h>

#include <core_mem_alloc.h>

namespace Genode {
	class Page_slab;
}

/**
 * Slab allocator returning aligned slab entries for page table descriptors.
 */
class Genode::Page_slab : public Genode::Allocator
{
	protected:

		static constexpr unsigned MIN_SLABS       = 6;
		static constexpr unsigned SLAB_SIZE       = get_page_size();
		static constexpr unsigned SLABS_PER_BLOCK = 8 * sizeof(addr_t);
		static constexpr unsigned ALIGN_LOG2      = get_page_size_log2();

		/**
		 * A slab block holding a fixed amount of slabs
		 */
		struct Slab_block
		{
			uint8_t                        data[SLAB_SIZE*SLABS_PER_BLOCK];
			Bit_allocator<SLABS_PER_BLOCK> indices;
			List_element<Slab_block>       list_elem;
			size_t                         ref_counter;

			Slab_block() : list_elem(this), ref_counter(0) {}

			/**
			 * Alloc a free block
			 */
			void* alloc()
			{
				ref_counter++;
				size_t off = indices.alloc() * SLAB_SIZE;
				return (void*)((Genode::addr_t)&data + off);
			}

			/**
			 * Free given slab
			 *
			 * \param addr  address of slab to free
			 * \return  true if slab is part of this block, and got freed
			 */
			bool free(void *addr)
			{
				if (addr < &data || addr > &indices) return false;
				ref_counter--;
				size_t off = (addr_t)addr - (addr_t)&data;
				indices.free(off / SLAB_SIZE);
				return true;
			}

			void * operator new (size_t, void * p) { return p; }
		};

		Slab_block _initial_sb __attribute__((aligned(1 << ALIGN_LOG2))); /*
		   first slab block is part of allocator to solve hen-egg problems */

		List<List_element<Slab_block> > _b_list; /* list of slab blocks */
		Core_mem_translator            *_backing_store; /* block allocator */
		size_t                          _free_slab_entries; /* free slabs  */
		bool                            _in_alloc;  /* in block allocation */

		/**
		 * Frees a given slab block
		 *
		 * \param b  address of slab block to free
		 */
		void _free_slab_block(Slab_block * b)
		{
			if (b == &_initial_sb) return;

			_b_list.remove(&b->list_elem);
			destroy(_backing_store, b);
			_free_slab_entries -= SLABS_PER_BLOCK;
		}

		/**
		 * Returns number of used slab blocks
		 */
		size_t _slab_blocks_in_use()
		{
			size_t cnt = 0;
			for (List_element<Slab_block> *le = _b_list.first();
			     le; le = le->next(), cnt++) ;
			return cnt;
		}

	public:

		class Out_of_slabs {};

		static constexpr size_t SLAB_BLOCK_SIZE = sizeof(Slab_block);

		/**
		 * Constructor
		 *
		 * \param backing_store  allocator for additional slab blocks
		 */
		Page_slab(Core_mem_translator *backing_store)
		: _backing_store(backing_store), _free_slab_entries(SLABS_PER_BLOCK),
		  _in_alloc(false) { _b_list.insert(&_initial_sb.list_elem); }

		~Page_slab()
		{
			while (_b_list.first() && (_b_list.first() != &_initial_sb.list_elem))
				_free_slab_block(_b_list.first()->object());
		}

		/**
		 * Set allocator used for slab blocks
		 */
		void backing_store(Core_mem_translator *cma) { _backing_store = cma; }

		/**
		 * Allocate additional slab blocks
		 *
		 * \throw Out_of_memory when no slab block could be allocated
		 */
		void alloc_slab_block()
		{
			void *p;
			if (!_backing_store->alloc_aligned(sizeof(Slab_block), &p,
			                                   ALIGN_LOG2).is_ok()) {
				throw Out_of_memory();
			}
			Slab_block *b = new (p) Slab_block();
			_b_list.insert(&b->list_elem);
			_free_slab_entries += SLABS_PER_BLOCK;
			_in_alloc = false;
		}

		/**
		 * Allocate a slab
		 *
		 * \throw Out_of_slabs when new slab blocks need to be allocated
		 * \returns pointer to new slab, or zero if allocation failed
		 */
		void *alloc()
		{
			if (_free_slab_entries <= MIN_SLABS && !_in_alloc) {
				_in_alloc = true;
				throw Out_of_slabs();
			}

			void * ret = 0;
			for (List_element<Slab_block> *le = _b_list.first();
			     le; le = le->next()) {
				if (le->object()->ref_counter == SLABS_PER_BLOCK)
					continue;

				ret = le->object()->alloc();
				_free_slab_entries--;
				return ret;
			}
			return ret;
		}

		/**
		 * Free a given slab
		 *
		 * As a side effect empty slab block might get freed
		 *
		 * \param addr  address of slab to free
		 */
		void free(void *addr)
		{
			for (List_element<Slab_block> *le = _b_list.first();
			     le; le = le->next()) {
				if (!le->object()->free(addr)) continue;

				if (_free_slab_entries++ > (MIN_SLABS+SLABS_PER_BLOCK)
				    && !le->object()->ref_counter)
					_free_slab_block(le->object());
			}
		}

		/**
		 * Return physical address of given slab address
		 *
		 * \param addr  slab address
		 */
		void * phys_addr(void * addr) {
			return _backing_store->phys_addr(addr); }

		/**
		 * Return slab address of given physical address
		 *
		 * \param addr  physical address
		 */
		void * virt_addr(void * addr) {
			return _backing_store->virt_addr(addr); }


		/************************
		 * Allocator interface **
		 ************************/

		bool   alloc(size_t, void **addr) { return (*addr = alloc()); }
		void   free(void *addr, size_t) { free(addr); }
		size_t consumed() { return SLAB_BLOCK_SIZE * _slab_blocks_in_use(); }
		size_t overhead(size_t) { return SLAB_BLOCK_SIZE/SLABS_PER_BLOCK; }
		bool   need_size_for_free() const override { return false; }
};

#endif /* _CORE__INCLUDE__PAGE_SLAB_H_ */
