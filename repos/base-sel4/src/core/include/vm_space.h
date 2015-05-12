/*
 * \brief   Virtual-memory space
 * \author  Norman Feske
 * \date    2015-05-04
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__VM_SPACE_H_
#define _CORE__INCLUDE__VM_SPACE_H_

/* Genode includes */
#include <util/bit_allocator.h>

/* core includes */
#include <page_table_registry.h>
#include <cnode.h>

namespace Genode { class Vm_space; }


class Genode::Vm_space
{
	private:

		Page_table_registry &_page_table_registry;

		unsigned const _id;
		unsigned const _pd_sel;

		Range_allocator &_phys_alloc;

		/**
		 * Maximum number of page tables and page frames for the VM space
		 */
		enum { NUM_VM_SEL_LOG2 = 13 };

		Cnode &_top_level_cnode;
		Cnode &_phys_cnode;

		/**
		 * 2nd-level CNode for aligning '_vm_cnode' with the LSB of the CSpace
		 */
		Cnode _vm_pad_cnode;

		/**
		 * 3rd-level CNode for storing page-table and page-frame capabilities
		 */
		Cnode _vm_cnode;

		/**
		 * Allocator for the selectors within '_vm_cnode'
		 */
		Bit_allocator<1UL << NUM_VM_SEL_LOG2> _sel_alloc;

		Lock _lock;

		/**
		 * Return selector for a capability slot within '_vm_cnode'
		 */
		unsigned _idx_to_sel(unsigned idx) const { return (_id << 20) | idx; }

		void _map_page(addr_t from_phys, addr_t to_virt)
		{
			/* allocate page-table entry selector */
			unsigned pte_idx = _sel_alloc.alloc();

			/*
			 * Copy page-frame selector to pte_sel
			 *
			 * This is needed because each page-frame selector can be
			 * inserted into only a single page table.
			 */
			_vm_cnode.copy(_phys_cnode, from_phys >> get_page_size_log2(), pte_idx);

			/* remember relationship between pte_sel and the virtual address */
			_page_table_registry.insert_page_table_entry(to_virt, pte_idx);

			/*
			 * Insert copy of page-frame selector into page table
			 */
			{
				seL4_IA32_Page          const service = _idx_to_sel(pte_idx);
				seL4_IA32_PageDirectory const pd      = _pd_sel;
				seL4_Word               const vaddr   = to_virt;
				seL4_CapRights          const rights  = seL4_AllRights;
				seL4_IA32_VMAttributes  const attr    = seL4_IA32_Default_VMAttributes;

				int const ret = seL4_IA32_Page_Map(service, pd, vaddr, rights, attr);

				if (ret != 0)
					PERR("seL4_IA32_Page_Map to 0x%lx returned %d",
					     (unsigned long)vaddr, ret);
			}
		}

		void _unmap_page(addr_t virt)
		{
			/* delete copy of the mapping's page-frame selector */
			_page_table_registry.apply(virt, [&] (unsigned idx) {
				_vm_cnode.remove(idx);
			});

			/* release meta data about the mapping */
			_page_table_registry.forget_page_table_entry(virt);
		}

		void _map_page_table(unsigned pt_sel, addr_t to_virt)
		{
			seL4_IA32_PageTable     const service = pt_sel;
			seL4_IA32_PageDirectory const pd      = _pd_sel;
			seL4_Word               const vaddr   = to_virt;
			seL4_IA32_VMAttributes  const attr    = seL4_IA32_Default_VMAttributes;

			int const ret = seL4_IA32_PageTable_Map(service, pd, vaddr, attr);

			if (ret != 0)
				PDBG("seL4_IA32_PageTable_Map returned %d", ret);
		}

		class Alloc_page_table_failed : Exception { };

		/**
		 * Allocate and install page table at given virtual address
		 *
		 * \throw Alloc_page_table_failed
		 */
		void _alloc_and_map_page_table(addr_t to_virt)
		{
			/* allocate page-table selector */
			unsigned const pt_idx = _sel_alloc.alloc();

			/* XXX account the consumed backing store */

			try {
				Kernel_object::create<Kernel_object::Page_table>(_phys_alloc,
				                                                 _vm_cnode.sel(),
				                                                 pt_idx);
			} catch (...) { throw Alloc_page_table_failed(); }

			unsigned const pt_sel = _idx_to_sel(pt_idx);

			_page_table_registry.insert_page_table(to_virt, pt_sel);

			_map_page_table(pt_sel, to_virt);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param pd_sel              selector for page directory
		 * \param vm_pad_cnode_sel    selector for the (2nd-level) VM pad CNode
		 * \param vm_cnode_sel        selector for the (3rd-level) VM CNode
		 * \param phys_alloc          backing store for the CNodes
		 * \param top_level_cnode     top-level CNode to insert 'vm_pad_cnode_sel'
		 * \param id                  ID used as index in 'top_level_cnode'
		 * \param page_table_registry association of VM CNode selectors with
		 *                            with virtual addresses
		 */
		Vm_space(unsigned             pd_sel,
		         unsigned             vm_pad_cnode_sel,
		         unsigned             vm_cnode_sel,
		         Range_allocator     &phys_alloc,
		         Cnode               &top_level_cnode,
		         Cnode               &core_cnode,
		         Cnode               &phys_cnode,
		         unsigned             id,
		         Page_table_registry &page_table_registry)
		:
			_page_table_registry(page_table_registry),
			_id(id), _pd_sel(pd_sel),
			_phys_alloc(phys_alloc),
			_top_level_cnode(top_level_cnode),
			_phys_cnode(phys_cnode),
			_vm_pad_cnode(core_cnode.sel(), vm_pad_cnode_sel,
			              32 - 12 - NUM_VM_SEL_LOG2, phys_alloc),
			_vm_cnode(core_cnode.sel(), vm_cnode_sel, NUM_VM_SEL_LOG2, phys_alloc)
		{
			Cnode_base const cspace(seL4_CapInitThreadCNode, 32);

			/* insert 3rd-level VM CNode into 2nd-level VM-pad CNode */
			_vm_pad_cnode.copy(cspace, vm_cnode_sel, 0);

			/* insert 2nd-level VM-pad CNode into 1st-level CNode */
			_top_level_cnode.copy(cspace, vm_pad_cnode_sel, id);
		}

		void map(addr_t from_phys, addr_t to_virt, size_t num_pages)
		{
			Lock::Guard guard(_lock);

			/* check if we need to add a page table to core's VM space */
			if (!_page_table_registry.has_page_table_at(to_virt))
				_alloc_and_map_page_table(to_virt);

			for (size_t i = 0; i < num_pages; i++) {
				off_t const offset = i << get_page_size_log2();
				_map_page(from_phys + offset, to_virt + offset);
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
