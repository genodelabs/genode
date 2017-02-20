/*
 * \brief  CPU driver for core
 * \author Norman Feske
 * \author Martin stein
 * \author Stefan Kalkowski
 * \date   2012-08-30
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <cpu.h>
#include <kernel/pd.h>
#include <kernel/kernel.h>

void Genode::Arm::clean_invalidate_data_cache() {
	asm volatile ("mcr p15, 0, %[rd], c7, c14, 0" :: [rd]"r"(0) : ); }


void Genode::Cpu::translation_added(Genode::addr_t const addr,
                                    Genode::size_t const size)
{
	using namespace Kernel;

	/*
	 * The Cortex-A8 CPU can't use the L1 cache on page-table
	 * walks. Therefore, as the page-tables lie in write-back cacheable
	 * memory we've to clean the corresponding cache-lines even when a
	 * page table entry is added. We only do this as core as the kernel
	 * adds translations solely before MMU and caches are enabled.
	 */
	if (is_user()) update_data_region(addr, size);
	else {
		Cpu * const cpu  = cpu_pool()->cpu(Cpu::executing_id());
		cpu->clean_invalidate_data_cache();
	}
}
