/*
 * \brief   Cpu driver implementations specific to ARM Cortex A15
 * \author  Stefan Kalkowski
 * \date    2016-01-07
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/pd.h>
#include <cpu.h>

void Genode::Arm_v7::enable_mmu_and_caches(Kernel::Pd & pd)
{
	Cpu::Mair0::write(Cpu::Mair0::init_virt_kernel());
	Cpu::Dacr::write(Cpu::Dacr::init_virt_kernel());
	Cpu::Ttbr0::write(Cpu::Ttbr0::init((Genode::addr_t)pd.translation_table(), pd.asid));
	Cpu::Ttbcr::write(Cpu::Ttbcr::init_virt_kernel());
	Cpu::Sctlr::enable_mmu_and_caches();
	invalidate_branch_predicts();
}
