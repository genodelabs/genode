/*
 * \brief  CPU driver for core Arm Cortex A8 specific implementation
 * \author Martin stein
 * \author Stefan Kalkowski
 * \date   2015-12-14
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <cpu.h>
#include <kernel/kernel.h>
#include <kernel/cpu.h>
#include <kernel/pd.h>

void Genode::Cpu::translation_table_insertions()
{
	clean_invalidate_data_cache();
	invalidate_tlb();
}


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


void Genode::Arm_v7::enable_mmu_and_caches(Kernel::Pd & pd)
{
	invalidate_tlb();
	Cpu::Cidr::write(pd.asid);
	Cpu::Dacr::write(Dacr::init_virt_kernel());
	Cpu::Ttbr0::write(Ttbr0::init((Genode::addr_t)pd.translation_table()));
	Cpu::Ttbcr::write(0);
	Cpu::Sctlr::enable_mmu_and_caches();
	invalidate_branch_predicts();
}
