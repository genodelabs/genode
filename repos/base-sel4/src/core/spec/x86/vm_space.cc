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

long Genode::Vm_space::_map_page(Genode::Cap_sel const &idx,
                                 Genode::addr_t const virt,
                                 Cache_attribute const cacheability,
                                 bool            const writable,
                                 bool            const, bool ept)
{
	seL4_X86_Page          const service = _idx_to_sel(idx.value());
	seL4_X86_PageDirectory const pd      = _pd_sel.value();
	seL4_CapRights_t       const rights  = writable ? seL4_ReadWrite : seL4_CanRead;
	seL4_X86_VMAttributes        attr    = seL4_X86_Default_VMAttributes;

	if (cacheability == UNCACHED)
		attr = seL4_X86_Uncacheable;
	else
	if (cacheability == WRITE_COMBINED)
		attr = seL4_X86_WriteCombining;

	if (ept)
		return seL4_X86_Page_MapEPT(service, pd, virt, rights, attr);
	else
		return seL4_X86_Page_Map(service, pd, virt, rights, attr);
}

long Genode::Vm_space::_unmap_page(Genode::Cap_sel const &idx)
{
	seL4_X86_Page const service = _idx_to_sel(idx.value());
	return seL4_X86_Page_Unmap(service);
}

long Genode::Vm_space::_invalidate_page(Genode::Cap_sel const &,
                                        seL4_Word const, seL4_Word const)
{
	return seL4_NoError;
}

/*******
 * EPT *
 *******/

enum {
	EPT_PAGE_TABLE_LOG2_SIZE  = 21, /*   2M  region */
	EPT_PAGE_DIR_LOG2_SIZE    = 30, /*   1GB region */
	EPT_PAGE_PDPT_LOG2_SIZE   = 39  /* 512GB region */
};

struct Ept_page_table_kobj
{
	enum { SEL4_TYPE = seL4_X86_EPTPTObject, SIZE_LOG2 = 12 };
	static char const *name() { return "ept page table"; }
};


struct Ept_page_directory_kobj
{
	enum { SEL4_TYPE = seL4_X86_EPTPDObject, SIZE_LOG2 = 12 };
	static char const *name() { return "ept page directory"; }
};

struct Ept_page_pointer_table_kobj
{
	enum { SEL4_TYPE = seL4_X86_EPTPDPTObject, SIZE_LOG2 = 12 };
	static char const *name() { return "ept page directory pointer table"; }
};

struct Ept_page_map_kobj
{
	enum { SEL4_TYPE = seL4_X86_EPTPML4Object, SIZE_LOG2 = 12 };
	static char const *name() { return "ept page-map level-4 table"; }
};

static long map_page_table(Genode::Cap_sel const pagetable,
                           Genode::Cap_sel const vroot,
                           Genode::addr_t  const virt)
{
	return seL4_X86_EPTPT_Map(pagetable.value(), vroot.value(), virt,
	                          seL4_X86_Default_VMAttributes);
}

static long map_pdpt(Genode::Cap_sel const pdpt,
                     Genode::Cap_sel const vroot,
                     Genode::addr_t  const virt)
{
	return seL4_X86_EPTPDPT_Map(pdpt.value(), vroot.value(), virt,
	                            seL4_X86_Default_VMAttributes);
}

static long map_directory(Genode::Cap_sel const pd,
                          Genode::Cap_sel const vroot,
                          Genode::addr_t  const virt)
{
	return seL4_X86_EPTPD_Map(pd.value(), vroot.value(), virt,
	                          seL4_X86_Default_VMAttributes);
}

void Genode::Vm_space::unsynchronized_alloc_guest_page_tables(addr_t const start,
                                                              addr_t size)
{
	addr_t constexpr PAGE_TABLE_AREA = 1UL << EPT_PAGE_TABLE_LOG2_SIZE;
	addr_t virt = start & ~(PAGE_TABLE_AREA - 1);
	for (; size != 0; size -= min(size, PAGE_TABLE_AREA), virt += PAGE_TABLE_AREA) {
		if (!_page_table_registry.page_level3_at(virt, EPT_PAGE_PDPT_LOG2_SIZE)) {
			/* 512 GB range - page directory pointer table */
			addr_t phys = 0;
			Cap_sel const pd = _alloc_and_map<Ept_page_pointer_table_kobj>(virt, map_pdpt, phys);
			try {
				_page_table_registry.insert_page_level3(virt, pd, phys, EPT_PAGE_PDPT_LOG2_SIZE);
			} catch (...) {
				_unmap_and_free(pd, phys);
				throw;
			}
		}

		if (!_page_table_registry.page_directory_at(virt, EPT_PAGE_DIR_LOG2_SIZE)) {
			/*   1 GB range - page directory */
			addr_t phys = 0;
			Cap_sel const pd = _alloc_and_map<Ept_page_directory_kobj>(virt, map_directory, phys);
			try {
				_page_table_registry.insert_page_directory(virt, pd, phys,
				                                           EPT_PAGE_DIR_LOG2_SIZE);
			} catch (...) {
				_unmap_and_free(pd, phys);
				throw;
			}
		}

		if (!_page_table_registry.page_table_at(virt, EPT_PAGE_TABLE_LOG2_SIZE)) {
			/*   2 MB range - page table */
			addr_t phys = 0;
			Cap_sel const pt = _alloc_and_map<Ept_page_table_kobj>(virt, map_page_table, phys);

			try {
				_page_table_registry.insert_page_table(virt, pt, phys,
			                                           EPT_PAGE_TABLE_LOG2_SIZE);
			} catch (...) {
				_unmap_and_free(pt, phys);
				throw;
			}
		}
	}
}
