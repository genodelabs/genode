/*
 * \brief Translation table definitions for core
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date 2012-02-22
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__CORTEX_A8__TRANSLATION_TABLE_H_
#define _CORE__SPEC__CORTEX_A8__TRANSLATION_TABLE_H_

#include <hw/spec/arm/page_table.h>
#include <kernel/interface.h>

#include <cpu.h>

constexpr unsigned Hw::Page_table::Descriptor_base::_device_tex() {
	return 2; }

constexpr bool Hw::Page_table::Descriptor_base::_smp() { return false; }

void Hw::Page_table::_table_changed(unsigned long addr, unsigned long size)
{
	/*
	 * The Cortex-A8 CPU can't use the L1 cache on page-table
	 * walks. Therefore, as the page-tables lie in write-back cacheable
	 * memory we've to clean the corresponding cache-lines even when a
	 * page table entry is added. We only do this as core as the kernel
	 * adds translations solely before MMU and caches are enabled.
	 */
	Genode::Cpu::clean_data_cache_by_virt_region(addr, size);
}

#endif /* _CORE__SPEC__CORTEX_A8__TRANSLATION_TABLE_H_ */
