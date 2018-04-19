/*
 * \brief   Virtual-memory space
 * \author  Norman Feske
 * \date    2015-05-04
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__VM_SPACE_H_
#define _CORE__INCLUDE__VM_SPACE_H_

/* Genode includes */
#include <util/bit_allocator.h>
#include <util/reconstructible.h>
#include <base/log.h>
#include <base/thread.h>
#include <base/session_label.h>

/* core includes */
#include <base/internal/capability_space_sel4.h>
#include <base/internal/stack_area.h>
#include <page_table_registry.h>
#include <cnode.h>
#include <cap_sel_alloc.h>
#include <core_cspace.h>

namespace Genode { class Vm_space; }


class Genode::Vm_space
{
	private:

		Session_label _pd_label;

		Cap_sel_alloc &_cap_sel_alloc;

		Page_table_registry &_page_table_registry;

		unsigned const _id;
		Cap_sel  const _pd_sel;

		Range_allocator &_phys_alloc;

		enum {
			/**
			 * Number of entries of 3rd-level VM CNode ('_vm_3rd_cnode')
			 */
			VM_3RD_CNODE_SIZE_LOG2 = (CONFIG_WORD_SIZE == 32) ? 8 : 7,

			/**
			 * Number of entries of each leaf CNodes
			 */
			LEAF_CNODE_SIZE_LOG2 = (CONFIG_WORD_SIZE == 32) ? 8 : 7,
			LEAF_CNODE_SIZE      = 1UL << LEAF_CNODE_SIZE_LOG2,

			/**
			 * Number of leaf CNodes
			 */
			NUM_LEAF_CNODES_LOG2 = (CONFIG_WORD_SIZE == 32) ? 6 : 5,
			NUM_LEAF_CNODES      = 1UL << NUM_LEAF_CNODES_LOG2,

			/**
			 * Maximum number of page tables and page frames for the VM space
			 */
			NUM_VM_SEL_LOG2 = LEAF_CNODE_SIZE_LOG2 + NUM_LEAF_CNODES_LOG2,

			/**
			 * Number of entries of the VM padding CNode ('_vm_pad_cnode')
			 */
			VM_PAD_CNODE_SIZE_LOG2 = 32 - Core_cspace::NUM_TOP_SEL_LOG2
			                       - VM_3RD_CNODE_SIZE_LOG2 - LEAF_CNODE_SIZE_LOG2,
		};

		Cnode &_top_level_cnode;
		Cnode &_phys_cnode;

		/**
		 * 2nd-level CNode for aligning '_vm_cnode' with the LSB of the CSpace
		 */
		Cnode _vm_pad_cnode;

		/**
		 * 3rd-level CNode that hosts the leaf CNodes
		 */
		Cnode _vm_3rd_cnode;

		/***
		 * 4th-level CNode for storing page-table and page-frame capabilities
		 */
		class Leaf_cnode
		{
			private:

				/*
				 * We leave the CNode unconstructed at the time of its
				 * instantiation to be able to create an array of 'Leaf_cnode'
				 * objects (where we cannot pass any arguments to the
				 * constructors of the individual objects).
				 */
				Constructible<Cnode> _cnode { };

			public:

				void construct(Cap_sel_alloc   &cap_sel_alloc,
				               Cap_sel          core_cnode_sel,
				               Range_allocator &phys_alloc)
				{
					_cnode.construct(core_cnode_sel, cap_sel_alloc.alloc(),
					                 LEAF_CNODE_SIZE_LOG2, phys_alloc);
				}

				void destruct(Cap_sel_alloc &cap_sel_alloc,
				               Range_allocator &phys_alloc)
				{
					_cnode->destruct(phys_alloc);
					cap_sel_alloc.free(_cnode->sel());
				}

				Cnode &cnode() { return *_cnode; }
		};

		Leaf_cnode _vm_cnodes[NUM_LEAF_CNODES];

		/**
		 * Allocator for the selectors within '_vm_cnodes'
		 */
		using Selector_allocator = Bit_allocator<1UL << NUM_VM_SEL_LOG2>;
		Selector_allocator _sel_alloc { };

		/**
		 * Return leaf CNode that contains an index allocated from '_sel_alloc'
		 */
		Cnode &_leaf_cnode(unsigned idx)
		{
			return _vm_cnodes[idx >> LEAF_CNODE_SIZE_LOG2].cnode();
		}

		/**
		 * Return entry within leaf cnode
		 */
		Cnode_index _leaf_cnode_entry(unsigned idx) const
		{
			return Cnode_index(idx & (LEAF_CNODE_SIZE - 1));
		}

		Lock _lock { };

		/**
		 * Return selector for a capability slot within '_vm_cnodes'
		 */
		addr_t _idx_to_sel(addr_t idx) const { return (_id << 20) | idx; }


		void _flush(bool const flush_support)
		{
			if (!flush_support) {
				warning("mapping cache full, but can't flush");
				throw;
			}

			warning("flush page table entries - mapping cache full - PD: ",
			        _pd_label.string());

			_page_table_registry.flush_pages([&] (Cap_sel const &idx,
			                                      addr_t const v_addr)
			{
				/* XXX - INITIAL_IPC_BUFFER can't be re-mapped currently */
				if (v_addr == 0x1000)
					return false;
				/* XXX - UTCB can't be re-mapped currently */
				if (stack_area_virtual_base() <= v_addr
				    && (v_addr < stack_area_virtual_base() +
				                 stack_area_virtual_size())
				    && !((v_addr + 0x1000) & (stack_virtual_size() - 1)))
						return false;

				long err = _unmap_page(idx);
				if (err != seL4_NoError)
					error("unmap failed, idx=", idx, " res=", err);

				_leaf_cnode(idx.value()).remove(_leaf_cnode_entry(idx.value()));

				_sel_alloc.free(idx.value());

				return true;
			});
		}


		bool _map_frame(addr_t const from_phys, addr_t const to_virt,
		                Cache_attribute const cacheability,
		                bool const writable, bool const executable,
		                bool const flush_support)
		{
			if (_page_table_registry.page_frame_at(to_virt)) {
				/*
				 * Valid behaviour if multiple threads concurrently
				 * causing the same page-fault. For the first thread the
				 * fault will be resolved, and the next thread will/would do
				 * it again. We just skip this attempt in order to avoid
				 * wasting of resources (idx selectors, creating kernel
				 * capabilities, causing kernel warning ...).
				 */
				return false;
			}
			/* allocate page-table-entry selector */
			addr_t pte_idx;
			try { pte_idx = _sel_alloc.alloc(); }
			catch (Selector_allocator::Out_of_indices) {

				/* free all page-table-entry selectors and retry once */
				_flush(flush_support);
				pte_idx = _sel_alloc.alloc();
			}

			/*
			 * Copy page-frame selector to pte_sel
			 *
			 * This is needed because each page-frame selector can be
			 * inserted into only a single page table.
			 */
			_leaf_cnode(pte_idx).copy(_phys_cnode,
			                          Cnode_index(from_phys >> get_page_size_log2()),
			                          Cnode_index(_leaf_cnode_entry(pte_idx)));

			/* remember relationship between pte_sel and the virtual address */
			try { _page_table_registry.insert_page_frame(to_virt, Cap_sel(pte_idx)); }
			catch (Page_table_registry::Mapping_cache_full) {

				/* free all entries of mapping cache and re-try once */
				_flush(flush_support);
				_page_table_registry.insert_page_frame(to_virt, Cap_sel(pte_idx));
			}

			/*
			 * Insert copy of page-frame selector into page table
			 */
			long ret = _map_page(Cap_sel(pte_idx), to_virt, cacheability,
			                     writable, executable);
			if (ret != seL4_NoError) {
				error("seL4_*_Page_Map ", Hex(from_phys), "->",
				      Hex(to_virt), " returned ", ret);
				return false;
			}
			return true;
		}

		/**
		 * Platform specific map/unmap of a page frame
		 */
		long _map_page(Genode::Cap_sel const &idx, Genode::addr_t const virt,
		               Cache_attribute const cacheability, bool const write,
		               bool const writable);
		long _unmap_page(Genode::Cap_sel const &idx);
		long _invalidate_page(Genode::Cap_sel const &, seL4_Word const,
		                      seL4_Word const);

		class Alloc_page_table_failed : Exception { };

		/**
		 * Allocate and install page structures for the protection domain.
		 *
		 * \throw Alloc_page_table_failed
		 */
		template <typename KOBJ>
		Cap_sel _alloc_and_map(addr_t const virt,
			                   long (&map_fn)(Cap_sel, Cap_sel, addr_t),
			                   addr_t &phys)
		{
			/* allocate page-* selector */
			addr_t const idx = _sel_alloc.alloc();

			try {
				phys = Untyped_memory::alloc_page(_phys_alloc);
				seL4_Untyped const service = Untyped_memory::untyped_sel(phys).value();
				create<KOBJ>(service, _leaf_cnode(idx).sel(),
				             _leaf_cnode_entry(idx));
			} catch (...) {
				/* XXX free idx, revert untyped memory, phys_addr, */
				 throw Alloc_page_table_failed();
			}

			Cap_sel const pt_sel(_idx_to_sel(idx));

			long const result = map_fn(pt_sel, _pd_sel, virt);
			if (result != seL4_NoError)
				error("seL4_*_Page*_Map(,", Hex(virt), ") returned ",
				      result);

			return Cap_sel(idx);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param pd_sel              selector for page directory
		 * \param cap_sel_alloc       capability-selector allocator used for
		 *                            VM CNodes
		 * \param phys_alloc          backing store for the CNodes
		 * \param top_level_cnode     top-level CNode to insert 'vm_pad_cnode_sel'
		 * \param id                  ID used as index in 'top_level_cnode'
		 * \param page_table_registry association of VM CNode selectors with
		 *                            with virtual addresses
		 */
		Vm_space(Cap_sel              pd_sel,
		         Cap_sel_alloc       &cap_sel_alloc,
		         Range_allocator     &phys_alloc,
		         Cnode               &top_level_cnode,
		         Cnode               &core_cnode,
		         Cnode               &phys_cnode,
		         unsigned             id,
		         Page_table_registry &page_table_registry,
		         const char *         label)
		:
			_pd_label(label),
			_cap_sel_alloc(cap_sel_alloc),
			_page_table_registry(page_table_registry),
			_id(id), _pd_sel(pd_sel),
			_phys_alloc(phys_alloc),
			_top_level_cnode(top_level_cnode),
			_phys_cnode(phys_cnode),
			_vm_pad_cnode(core_cnode.sel(),
			              Cnode_index(_cap_sel_alloc.alloc().value()),
			              VM_PAD_CNODE_SIZE_LOG2, phys_alloc),
			_vm_3rd_cnode(core_cnode.sel(),
			              Cnode_index(_cap_sel_alloc.alloc().value()),
			              VM_3RD_CNODE_SIZE_LOG2, phys_alloc)
		{
			Cnode_base const cspace(Cap_sel(seL4_CapInitThreadCNode), 32);

			for (unsigned i = 0; i < NUM_LEAF_CNODES; i++) {

				/* init leaf VM CNode */
				_vm_cnodes[i].construct(_cap_sel_alloc, core_cnode.sel(), phys_alloc);

				/* insert leaf VM CNode into 3nd-level VM CNode */
				_vm_3rd_cnode.copy(cspace, _vm_cnodes[i].cnode().sel(), Cnode_index(i));
			}

			/* insert 3rd-level VM CNode into 2nd-level VM-pad CNode */
			_vm_pad_cnode.copy(cspace, _vm_3rd_cnode.sel(), Cnode_index(0));

			/* insert 2nd-level VM-pad CNode into 1st-level CNode */
			_top_level_cnode.copy(cspace, _vm_pad_cnode.sel(), Cnode_index(_id));
		}

		~Vm_space()
		{
			/* delete copy of the mapping's page-frame selectors */
			_page_table_registry.flush_all([&] (Cap_sel const &idx, addr_t const virt) {

				long err = _unmap_page(idx);
				if (err != seL4_NoError)
					error("unmap ", Hex(virt), " failed, ", idx, " res=", err);

				_leaf_cnode(idx.value()).remove(_leaf_cnode_entry(idx.value()));

				_sel_alloc.free(idx.value());

				return true;
			}, [&] (Cap_sel const &idx, addr_t const paddr) {

				_leaf_cnode(idx.value()).remove(idx);

				_sel_alloc.free(idx.value());

				Untyped_memory::free_page(_phys_alloc, paddr);
			});

			for (unsigned i = 0; i < NUM_LEAF_CNODES; i++) {
				_vm_3rd_cnode.remove(Cnode_index(i));
				_vm_cnodes[i].destruct(_cap_sel_alloc, _phys_alloc);
			}
			_vm_pad_cnode.remove(Cnode_index(0));
			_top_level_cnode.remove(Cnode_index(_id));

			_vm_3rd_cnode.destruct(_phys_alloc);
			_vm_pad_cnode.destruct(_phys_alloc);

			_cap_sel_alloc.free(_vm_3rd_cnode.sel());
			_cap_sel_alloc.free(_vm_pad_cnode.sel());
		}

		void map(addr_t const from_phys, addr_t const to_virt,
		         size_t const num_pages, Cache_attribute const cacheability,
		         bool const writable, bool const executable, bool flush_support)
		{
			Lock::Guard guard(_lock);

			for (size_t i = 0; i < num_pages; i++) {
				off_t const offset = i << get_page_size_log2();

				if (!_map_frame(from_phys + offset, to_virt + offset,
				                cacheability, writable, executable,
				                flush_support))
					warning("mapping failed ", Hex(from_phys + offset),
					        " -> ", Hex(to_virt + offset), " ",
					        !flush_support ? "core" : "");
			}
		}

		bool unmap(addr_t const virt, size_t const num_pages,
		           bool const invalidate = false)
		{
			bool unmap_success = true;

			Lock::Guard guard(_lock);

			for (size_t i = 0; unmap_success && i < num_pages; i++) {
				off_t const offset = i << get_page_size_log2();

				_page_table_registry.flush_page(virt + offset, [&] (Cap_sel const &idx, addr_t) {

					if (invalidate) {
						long result = _invalidate_page(idx, virt + offset,
						                               virt + offset + 4096);
						if (result != seL4_NoError)
							error("invalidating ", Hex(virt + offset),
							      " failed, idx=", idx, " result=", result);
					}

					long result = _unmap_page(idx);
					if (result != seL4_NoError) {
						error("unmap ", Hex(virt + offset), " failed, idx=",
						      idx, " result=", result);
						unmap_success = false;
					}

					_leaf_cnode(idx.value()).remove(_leaf_cnode_entry(idx.value()));

					_sel_alloc.free(idx.value());
				});
			}
			return unmap_success;
		}

		void unsynchronized_alloc_page_tables(addr_t const start,
		                                      addr_t const size);

		void alloc_page_tables(addr_t const start, addr_t const size)
		{
			Lock::Guard guard(_lock);
			unsynchronized_alloc_page_tables(start, size);
		}

		Session_label const & pd_label() const { return _pd_label; }
};

#endif /* _CORE__INCLUDE__VM_SPACE_H_ */
