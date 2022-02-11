/*
 * \brief  CPU driver for core
 * \author Stefan Kalkowski
 * \date   2019-05-10
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__ARM_V8__CPU_H_
#define _CORE__SPEC__ARM_V8__CPU_H_

/* base includes */
#include <util/register.h>
#include <cpu/cpu_state.h>

/* base internal includes */
#include <base/internal/align_at.h>

/* base-hw internal includes */
#include <hw/spec/arm_64/cpu.h>

/* base-hw Core includes */
#include <spec/arm_v8/address_space_id_allocator.h>
#include <spec/arm_v8/translation_table.h>

namespace Kernel { struct Thread_fault; }


namespace Board { class Address_space_id_allocator; }


namespace Genode {

	struct Cpu;
	using sizet_arithm_t = __uint128_t;
	using uint128_t      = __uint128_t;
}


struct Genode::Cpu : Hw::Arm_64_cpu
{
	enum Exception_entry {
		SYNC_LEVEL_EL1          = 0x000,
		IRQ_LEVEL_EL1           = 0x080,
		FIQ_LEVEL_EL1           = 0x100,
		SERR_LEVEL_EL1          = 0x180,
		SYNC_LEVEL_EL1_EXC_MODE = 0x200,
		IRQ_LEVEL_EL1_EXC_MODE  = 0x280,
		FIQ_LEVEL_EL1_EXC_MODE  = 0x300,
		SERR_LEVEL_EL1_EXC_MODE = 0x380,
		SYNC_LEVEL_EL0          = 0x400,
		IRQ_LEVEL_EL0           = 0x480,
		FIQ_LEVEL_EL0           = 0x500,
		SERR_LEVEL_EL0          = 0x580,
		AARCH32_SYNC_LEVEL_EL0  = 0x600,
		AARCH32_IRQ_LEVEL_EL0   = 0x680,
		AARCH32_FIQ_LEVEL_EL0   = 0x700,
		AARCH32_SERR_LEVEL_EL0  = 0x780,
		RESET                   = 0x800
	};

	struct alignas(16) Fpu_state
	{
		Genode::uint128_t q[32];
		Genode::uint64_t  fpsr;
		Genode::uint64_t  fpcr;
	};

	struct alignas(16) Context : Cpu_state
	{
		Genode::uint64_t pstate { };
		Genode::uint64_t exception_type { RESET };
		Fpu_state        fpu_state { };

		Context(bool privileged);
	};

	class Mmu_context
	{
		private:

			Board::Address_space_id_allocator &_addr_space_id_alloc;

		public:

			Ttbr::access_t ttbr;

			Mmu_context(addr_t                             page_table_base,
			            Board::Address_space_id_allocator &addr_space_id_alloc);

			~Mmu_context();

			Genode::uint16_t id() {
				return Ttbr::Asid::get(ttbr) & 0xffff; }
	};

	void switch_to(Context&, Mmu_context &);

	static void mmu_fault(Context &, Kernel::Thread_fault &);

	/**
	 * Return kernel name of the executing CPU
	 */
	static unsigned executing_id() { return Cpu::Mpidr::read() & 0xff; }

	static size_t cache_line_size();
	static void clear_memory_region(addr_t const addr,
	                                size_t const size,
	                                bool changed_cache_properties);

	static void cache_coherent_region(addr_t const addr,
	                                  size_t const size);
	static void cache_clean_invalidate_data_region(addr_t const addr,
	                                               size_t const size);
	static void cache_invalidate_data_region(addr_t const addr,
	                                         size_t const size);
};

#endif /* _CORE__SPEC__ARM_V8__CPU_H_ */
