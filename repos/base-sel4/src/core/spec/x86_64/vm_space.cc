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

/* core includes */
#include <vm_space.h>

#include "arch_kernel_object.h"

using namespace Core;


static long map_page_table(Cap_sel const pagetable,
                           Cap_sel const vroot,
                           addr_t  const virt)
{
	return seL4_X86_PageTable_Map(pagetable.value(), vroot.value(), virt,
	                              seL4_X86_Default_VMAttributes);
}

static long map_pdpt(Cap_sel   const pdpt,
                     Cap_sel   const vroot,
                     seL4_Word const virt)
{
	return seL4_X86_PDPT_Map(pdpt.value(), vroot.value(), virt,
	                         seL4_X86_Default_VMAttributes);
}

static long map_directory(Cap_sel   const pd,
                          Cap_sel   const vroot,
                          seL4_Word const virt)
{
	return seL4_X86_PageDirectory_Map(pd.value(), vroot.value(), virt,
	                                  seL4_X86_Default_VMAttributes);
}

bool Vm_space::unsynchronized_alloc_page_tables(addr_t const start,
                                                addr_t const size)
{
	addr_t constexpr PAGE_TABLE_AREA = 1UL << PAGE_TABLE_LOG2_SIZE;
	addr_t virt = start & ~(PAGE_TABLE_AREA - 1);

	for (; virt < start + size; virt += PAGE_TABLE_AREA) {
		if (!_pt_registry.page_level3_at(virt, PAGE_PDPT_LOG2_SIZE)) {
			/* 512 GB range - page directory pointer table */
			auto result = _alloc_and_map<Page_pointer_table_kobj>(virt,
				[&](Cap_sel const pdpt, Cap_sel const vroot, addr_t const pguest,
				    Cap_sel const pd, addr_t const phys) {

				if (map_pdpt(pdpt, vroot, pguest) != seL4_NoError)
					return Vm_space::Result(Alloc_error::DENIED);

				return _pt_registry.insert_page_level3(virt, pd, phys,
				                                       PAGE_PDPT_LOG2_SIZE);
			});

			if (result.failed())
				return false;
		}
		if (!_pt_registry.page_directory_at(virt, PAGE_DIR_LOG2_SIZE)) {
			/*   1 GB range - page directory */
			auto result = _alloc_and_map<Page_directory_kobj>(virt,
				[&](Cap_sel const pdpt, Cap_sel const vroot, addr_t const pguest,
				    Cap_sel const pd, addr_t const phys) {

				if (map_directory(pdpt, vroot, pguest) != seL4_NoError)
					return Vm_space::Result(Alloc_error::DENIED);

				return _pt_registry.insert_page_directory(virt, pd, phys,
				                                          PAGE_DIR_LOG2_SIZE);
			});

			if (result.failed())
				return false;
		}

		if (!_pt_registry.page_table_at(virt, PAGE_TABLE_LOG2_SIZE)) {
			/*   2 MB range - page table */
			auto result = _alloc_and_map<Page_table_kobj>(virt,
				[&](Cap_sel const pdpt, Cap_sel const vroot, addr_t const pguest,
				    Cap_sel const pt, addr_t const phys) {

				if (map_page_table(pdpt, vroot, pguest) != seL4_NoError)
					return Vm_space::Result(Alloc_error::DENIED);

				return _pt_registry.insert_page_table(virt, pt, phys,
				                                      PAGE_TABLE_LOG2_SIZE);
			});

			if (result.failed())
				return false;
		}
	}

	return true;
}
