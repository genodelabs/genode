/*
 * \brief   Kernel PD object implementations specific to ARM
 * \author  Stefan Kalkowski
 * \date    2018-11-22
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <kernel/cpu.h>
#include <kernel/pd.h>

bool Kernel::Pd::invalidate_tlb(Cpu & cpu, addr_t, size_t)
{
	/* invalidate the TLB on the local CPU only */
	if (cpu.id() == Cpu::executing_id()) {
		Cpu::invalidate_tlb(mmu_regs.id());
	}

	/*
	 * on all SMP ARM platforms we support the TLB can be maintained
	 * cross-cpu coherently
	 */
	return false;
}
