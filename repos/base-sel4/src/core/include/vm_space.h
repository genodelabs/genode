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
			VM_3RD_CNODE_SIZE_LOG2 = 8,

			/**
			 * Number of entries of each leaf CNodes
			 */
			LEAF_CNODE_SIZE_LOG2 = 8UL,
			LEAF_CNODE_SIZE      = 1UL << LEAF_CNODE_SIZE_LOG2,

			/**
			 * Number of leaf CNodes
			 */
			NUM_LEAF_CNODES_LOG2 = 6UL,
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
				Constructible<Cnode> _cnode;

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
		Bit_allocator<1UL << NUM_VM_SEL_LOG2> _sel_alloc;

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

		Lock _lock;

		/**
		 * Return selector for a capability slot within '_vm_cnodes'
		 */
		unsigned _idx_to_sel(unsigned idx) const { return (_id << 20) | idx; }

		bool _map_page(addr_t from_phys, addr_t to_virt, bool flush_support)
		{
			/* allocate page-table entry selector */
			unsigned pte_idx = _sel_alloc.alloc();

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
			try {
				_page_table_registry.insert_page_table_entry(to_virt, pte_idx);
			} catch (Page_table_registry::Mapping_cache_full) {
				if (!flush_support)
					throw;

				warning("flush page table entries - mapping cache full - PD: ",
				        _pd_label.string());

				_page_table_registry.flush_cache();

				/* re-try once */
				_page_table_registry.insert_page_table_entry(to_virt, pte_idx);
			}

			/*
			 * Insert copy of page-frame selector into page table
			 */
			{
				seL4_X86_Page          const service = _idx_to_sel(pte_idx);
				seL4_X86_PageDirectory const pd      = _pd_sel.value();
				seL4_Word              const vaddr   = to_virt;
				seL4_CapRights         const rights  = seL4_AllRights;
				seL4_X86_VMAttributes  const attr    = seL4_X86_Default_VMAttributes;

				int const ret = seL4_X86_Page_Map(service, pd, vaddr, rights, attr);

				if (ret != seL4_NoError) {
					error("seL4_X86_Page_Map to ", Hex(from_phys), "->",
					      Hex(to_virt), " returned ", ret);
					return false;
				}
			}
			return true;
		}

		void _unmap_page(addr_t virt)
		{
			/* delete copy of the mapping's page-frame selector */
			_page_table_registry.apply(virt, [&] (unsigned idx) {

			 	_leaf_cnode(idx).remove(_leaf_cnode_entry(idx));

				_sel_alloc.free(idx);
			});

			/* release meta data about the mapping */
			_page_table_registry.forget_page_table_entry(virt);
		}

		void _map_page_table(Cap_sel pt_sel, addr_t to_virt)
		{
			seL4_X86_PageTable     const service = pt_sel.value();
			seL4_X86_PageDirectory const pd      = _pd_sel.value();
			seL4_Word              const vaddr   = to_virt;
			seL4_X86_VMAttributes  const attr    = seL4_X86_Default_VMAttributes;

			int const ret = seL4_X86_PageTable_Map(service, pd, vaddr, attr);
			if (ret != seL4_NoError)
				error("seL4_X86_PageTable_Map returned ", ret);
		}

		class Alloc_page_table_failed : Exception { };

		/**
		 * Allocate and install page table at given virtual address
		 *
		 * \throw Alloc_page_table_failed
		 */
		void _alloc_and_map_page_table(addr_t to_virt, bool core_vm)
		{
			/* allocate page-table selector */
			unsigned const pt_idx = _sel_alloc.alloc();

			/* XXX account the consumed backing store */

			try {
				/* XXX evil manual unlock/lock pattern */
				if (core_vm)
					_lock.unlock();

				create<Page_table_kobj>(_phys_alloc,
				                        _leaf_cnode(pt_idx).sel(),
				                        _leaf_cnode_entry(pt_idx));

				if (core_vm)
					_lock.lock();
			} catch (...) {
				if (core_vm)
					_lock.lock();
				 throw Alloc_page_table_failed();
			}

			Cap_sel const pt_sel(_idx_to_sel(pt_idx));

			_page_table_registry.insert_page_table(to_virt, pt_sel);

			_map_page_table(pt_sel, to_virt);
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
			_page_table_registry.apply_to_and_destruct_all([&] (unsigned idx) {
				_leaf_cnode(idx).remove(_leaf_cnode_entry(idx));

				_sel_alloc.free(idx);
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

		void map(addr_t from_phys, addr_t to_virt, size_t num_pages,
		         bool flush_support = true)
		{
			Lock::Guard guard(_lock);

			for (size_t i = 0; i < num_pages; i++) {
				off_t const offset = i << get_page_size_log2();

				/* check if we need to add a page table to core's VM space */
				if (!_page_table_registry.has_page_table_at(to_virt + offset))
					_alloc_and_map_page_table(to_virt + offset, !flush_support);

				if (_map_page(from_phys + offset, to_virt + offset, flush_support))
					continue;

				/* XXX - Why this quirk is necessary - shouldn't happen ?!? */

				error("mapping failed - retry once ", Hex(from_phys + offset),
				      " -> ", Hex(to_virt + offset));

				_page_table_registry.forget_page_table_entry(to_virt + offset);
				_alloc_and_map_page_table(to_virt + offset, !flush_support);

				_map_page(from_phys + offset, to_virt + offset, flush_support);
			}
		}

		void unmap(addr_t virt, size_t num_pages)
		{
			Lock::Guard guard(_lock);

			for (size_t i = 0; i < num_pages; i++) {
				off_t const offset = i << get_page_size_log2();
				_unmap_page(virt + offset);
			}
		}
};

#endif /* _CORE__INCLUDE__VM_SPACE_H_ */
