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

static long map_page_table(Genode::Cap_sel const pagetable,
                           Genode::Cap_sel const vroot,
                           Genode::addr_t const virt)
{
	return seL4_ARM_PageTable_Map(pagetable.value(), vroot.value(), virt,
	                              seL4_ARM_Default_VMAttributes);
}

long Genode::Vm_space::_map_page(Genode::Cap_sel const &idx,
                                 Genode::addr_t  const virt,
                                 Cache_attribute const cacheability,
                                 bool            const writable,
                                 bool            const executable,
                                 bool)
{
	seL4_ARM_Page          const service = _idx_to_sel(idx.value());
	seL4_ARM_PageDirectory const pd      = _pd_sel.value();
	seL4_CapRights_t       const rights  = writable ? seL4_ReadWrite : seL4_CanRead;
	seL4_ARM_VMAttributes        attr    = executable ? seL4_ARM_Default_VMAttributes : seL4_ARM_Default_VMAttributes_NoExecute;

	if (cacheability == UNCACHED)
		attr = seL4_ARM_Uncacheable;

	return seL4_ARM_Page_Map(service, pd, virt, rights, attr);
}

long Genode::Vm_space::_unmap_page(Genode::Cap_sel const &idx)
{
	seL4_ARM_Page const service = _idx_to_sel(idx.value());
	return seL4_ARM_Page_Unmap(service);
}

long Genode::Vm_space::_invalidate_page(Genode::Cap_sel const &idx,
                                        seL4_Word const start,
                                        seL4_Word const end)
{
	seL4_ARM_Page const service = _idx_to_sel(idx.value());
	long error = seL4_ARM_Page_CleanInvalidate_Data(service, 0, end - start);

	if (error == seL4_NoError) {
		seL4_ARM_PageDirectory const pd = _pd_sel.value();
		error = seL4_ARM_PageDirectory_CleanInvalidate_Data(pd, start, end);
	}

	return error;
}

void Genode::Vm_space::unsynchronized_alloc_page_tables(addr_t const start,
                                                        addr_t const size)
{
	addr_t constexpr PAGE_TABLE_AREA = 1UL << PAGE_TABLE_LOG2_SIZE;
	addr_t virt = start & ~(PAGE_TABLE_AREA - 1);
	for (; virt < start + size; virt += PAGE_TABLE_AREA) {

		if (_page_table_registry.page_table_at(virt, PAGE_TABLE_LOG2_SIZE))
			continue;

		addr_t phys = 0;

		/* 1 MB range - page table */
		Cap_sel const pt = _alloc_and_map<Page_table_kobj>(virt, map_page_table, phys);
		try {
			_page_table_registry.insert_page_table(virt, pt, phys,
			                                       PAGE_TABLE_LOG2_SIZE);
		} catch (...) {
			_unmap_and_free(pt, phys);
			throw;
		}
	}
}
