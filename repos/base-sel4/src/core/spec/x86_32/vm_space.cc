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

void Vm_space::unsynchronized_alloc_page_tables(addr_t const start,
                                                addr_t const size)
{
	addr_t constexpr PAGE_TABLE_AREA = 1UL << PAGE_TABLE_LOG2_SIZE;
	addr_t virt = start & ~(PAGE_TABLE_AREA - 1);
	for (; virt < start + size; virt += PAGE_TABLE_AREA) {

		if (_page_table_registry.page_table_at(virt, PAGE_TABLE_LOG2_SIZE))
			continue;

		addr_t phys = 0;

		/* 4 MB range - page table */
		Cap_sel const pt = _alloc_and_map<Page_table_kobj>(virt, map_page_table, phys);
		_page_table_registry.insert_page_table(virt, pt, phys,
		                                       PAGE_TABLE_LOG2_SIZE);
	}
}
