/*
 * \brief  CPU driver for core
 * \author Martin stein
 * \author Stefan Kalkowski
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2011-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <kernel/pd.h>
#include <cpu.h>

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
