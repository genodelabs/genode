/*
 * \brief  CPU driver for core
 * \author Martin stein
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__ARM_V7__CPU_SUPPORT_H_
#define _CORE__SPEC__ARM_V7__CPU_SUPPORT_H_

/* core includes */
#include <spec/arm/cpu_support.h>

namespace Genode { struct Arm_v7_cpu; }


struct Genode::Arm_v7_cpu : Arm_cpu
{
	/**
	 * Returns whether this cpu implements the multiprocessor extensions
	 */
	static bool multi_processor()
	{
		static bool mp = Mpidr::Me::get(Mpidr::read());
		return mp;
	}

	/**
	 * Invalidate TLB for given address space id
	 */
	static void invalidate_tlb(unsigned asid)
	{
		if (multi_processor()) {
			if (asid) Tlbiasidis::write(asid);
			else      Tlbiallis::write(0);
		} else Arm_cpu::invalidate_tlb(asid);
	}

	static inline void synchronization_barrier()
	{
		asm volatile("dsb sy\n"
		             "isb sy\n" ::: "memory");
	}
};

#endif /* _CORE__SPEC__ARM_V7__CPU_SUPPORT_H_ */
