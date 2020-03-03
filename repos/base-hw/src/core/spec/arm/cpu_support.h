/*
 * \brief  CPU driver for core
 * \author Martin stein
 * \author Stefan Kalkowski
 * \date   2012-09-11
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__ARM__CPU_SUPPORT_H_
#define _CORE__SPEC__ARM__CPU_SUPPORT_H_

/* Genode includes */
#include <util/register.h>
#include <cpu/cpu_state.h>
#include <base/internal/align_at.h>

#include <hw/spec/arm/cpu.h>

/* local includes */
#include <kernel/interface_support.h>
#include <kernel/kernel.h>
#include <board.h>
#include <util.h>

namespace Kernel { struct Thread_fault; }

namespace Genode {
	using sizet_arithm_t = Genode::uint64_t;
	struct Arm_cpu;
}

struct Genode::Arm_cpu : public Hw::Arm_cpu
{
	struct Fpu_context
	{
		uint32_t fpscr { 1UL << 24 }; /* VFP/SIMD - status/control register     */
		uint64_t d0_d31[32];  /* VFP/SIMD - general purpose registers   */
	};

	struct alignas(4) Context : Cpu_state, Fpu_context
	{
		Context(bool privileged);
	};

	/**
	 * This class comprises ARM specific protection domain attributes
	 */
	struct Mmu_context
	{
		Cidr::access_t  cidr;
		Ttbr0::access_t ttbr0;

		Mmu_context(addr_t page_table_base);
		~Mmu_context();

		uint8_t id() { return cidr; }
	};

	/**
	 * Invalidate all entries of all instruction caches
	 */
	static void invalidate_instr_cache() {
		asm volatile ("mcr p15, 0, %0, c7, c5, 0" :: "r" (0) : ); }

	/**
	 * Clean data-cache for virtual region 'base' - 'base + size'
	 */
	static void clean_data_cache_by_virt_region(addr_t const base,
	                                            size_t const size);

	/**
	 * Clean and invalidate data-cache for virtual region
	 * 'base' - 'base + size'
	 */
	static void clean_invalidate_data_cache_by_virt_region(addr_t const base,
	                                                       size_t const size);

	static void clear_memory_region(addr_t const addr,
	                                size_t const size,
	                                bool changed_cache_properties);

	static void cache_coherent_region(addr_t const addr,
	                                  size_t const size);

	/**
	 * Invalidate TLB regarding the given address space id
	 */
	static void invalidate_tlb(unsigned asid)
	{
		if (asid) Tlbiasid::write(asid);
		else      Tlbiall::write(0);
	}

	void switch_to(Context&, Mmu_context & o);

	static void mmu_fault(Context & c, Kernel::Thread_fault & fault);
	static void mmu_fault_status(Fsr::access_t fsr,
	                             Kernel::Thread_fault & fault);


	/*************
	 ** Dummies **
	 *************/

	/**
	 * Return kernel name of the executing CPU
	 */
	static unsigned executing_id() { return 0; }
};

#endif /* _CORE__SPEC__ARM__CPU_SUPPORT_H_ */
