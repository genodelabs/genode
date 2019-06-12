/*
 * \brief  X86-specific implementations for the kernel PD object
 * \author Stefan Kalkowski
 * \date   2018-11-22
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
	/* on the current CPU invalidate the TLB */
	if (cpu.id() == Cpu::executing_id()) {
		Cpu::invalidate_tlb();
		return false;
	}

	/* for all other cpus send an IPI */
	cpu.trigger_ip_interrupt();
	return true;
}
