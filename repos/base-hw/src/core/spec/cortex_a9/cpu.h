/*
 * \brief  CPU driver for core
 * \author Martin stein
 * \author Stefan Kalkowski
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__CORTEX_A9__CPU_H_
#define _CORE__SPEC__CORTEX_A9__CPU_H_

/* core includes */
#include <spec/arm/fpu.h>
#include <spec/arm_v7/cpu_support.h>
#include <board.h>

namespace Genode { class Cpu; }

class Genode::Cpu : public Arm_v7_cpu
{
	protected:

		Fpu _fpu;

	public:

		struct Context : Arm_cpu::Context, Fpu::Context
		{
			Context(bool privileged)
			: Arm_cpu::Context(privileged) {}
		};

		/**
		 * Next cpu context to switch to
		 *
		 * \param context  context to switch to
		 */
		void switch_to(Context & context, Mmu_context & mmu_context)
		{
			Arm_cpu::switch_to(context, mmu_context);
			_fpu.switch_to(context);
		}

		/**
		 * Return wether to retry an undefined user instruction after this call
		 *
		 * \param state  CPU state of the user
		 */
		bool retry_undefined_instr(Context & context) {
			return _fpu.fault(context); }

		/**
		 * Write back dirty cache lines and invalidate whole data cache
		 */
		void clean_invalidate_data_cache()
		{
			clean_invalidate_inner_data_cache();
			Board::l2_cache().clean_invalidate();
		}

		/**
		 * Invalidate whole data cache
		 */
		void invalidate_data_cache()
		{
			invalidate_inner_data_cache();
			Board::l2_cache().invalidate();
		}

		/**
		 * Clean and invalidate data-cache for virtual region
		 * 'base' - 'base + size'
		 */
		void clean_invalidate_data_cache_by_virt_region(addr_t base,
		                                                size_t const size)
		{
			Arm_cpu::clean_invalidate_data_cache_by_virt_region(base, size);
			Board::l2_cache().clean_invalidate();
		}

		static unsigned executing_id() { return Mpidr::Aff_0::get(Mpidr::read()); }
};

#endif /* _CORE__SPEC__CORTEX_A9__CPU_H_ */
