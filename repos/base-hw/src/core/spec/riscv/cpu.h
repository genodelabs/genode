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

/* base includes */
#include <base/stdint.h>
#include <cpu/cpu_state.h>
#include <util/register.h>

/* base internal includes */
#include <base/internal/align_at.h>

/* base-hw internal includes */
#include <hw/spec/riscv/cpu.h>
#include <hw/spec/riscv/page_table.h>

/* base-hw Core includes */
#include <kernel/interface.h>
#include <spec/riscv/address_space_id_allocator.h>

namespace Kernel { struct Thread_fault; }


namespace Board { class Address_space_id_allocator; }


namespace Genode {

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

		class Mmu_context
		{
			private:

				Board::Address_space_id_allocator &_addr_space_id_alloc;

			public:

				Satp::access_t satp = 0;

				Mmu_context(addr_t                             page_table_base,
				            Board::Address_space_id_allocator &addr_space_id_alloc);

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
			asm volatile ("sfence.vma\n");
		}

		static void invalidate_tlb_by_pid(unsigned const /* pid */) { sfence(); }

		void switch_to(Mmu_context & context);
		static void mmu_fault(Context & c, Kernel::Thread_fault & f);

		static unsigned executing_id() { return 0; }

		static void clear_memory_region(addr_t const addr,
		                                size_t const size,
		                                bool changed_cache_properties);
};


template <typename E, unsigned B, unsigned S>
void Sv39::Level_x_translation_table<E, B, S>::_translation_added(addr_t, size_t)
{
	Genode::Cpu::sfence();
}


#endif /* _CORE__SPEC__RISCV__CPU_H_ */
