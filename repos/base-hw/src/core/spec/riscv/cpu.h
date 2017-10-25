/*
 * \brief  CPU driver for core
 * \author Sebastian Sumpf
 * \date   2015-06-02
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__RISCV__CPU_H_
#define _CORE__SPEC__RISCV__CPU_H_

/* Genode includes */
#include <base/stdint.h>
#include <cpu/cpu_state.h>
#include <util/register.h>

#include <base/internal/align_at.h>

#include <kernel/interface.h>
#include <hw/spec/riscv/cpu.h>

namespace Kernel { struct Thread_fault; }

namespace Genode
{
	/**
	 * CPU driver for core
	 */
	class Cpu;

	typedef __uint128_t sizet_arithm_t;
}

namespace Kernel { class Pd; }

class Genode::Cpu : public Hw::Riscv_cpu
{
	public:

		struct alignas(8) Context : Cpu_state
		{
			Context(bool);
		};

		struct Mmu_context
		{
			Sptbr::access_t sptbr;

			Mmu_context(addr_t page_table_base);
			~Mmu_context();
		};


		/**
		 * From the manual
		 *
		 * The behavior of SFENCE.VM depends on the current value of the sasid
		 * register. If sasid is nonzero, SFENCE.VM takes effect only for address
		 * translations in the current address space. If sasid is zero, SFENCE.VM
		 * affects address translations for all address spaces. In this case, it
		 * also affects global mappings, which are described in Section 4.5.1.
		 *
		 * Right no we will flush anything
		 */
		static void sfence()
		{
			/*
			 * Note: In core the address space id must be zero
			 */
			asm volatile ("sfence.vm\n");
		}

		static void invalidate_tlb_by_pid(unsigned const pid) { sfence(); }

		void switch_to(Mmu_context & context);
		static void mmu_fault(Context & c, Kernel::Thread_fault & f);

		static unsigned executing_id() { return 0; }
		static unsigned primary_id()   { return 0; }
};

#endif /* _CORE__SPEC__RISCV__CPU_H_ */
