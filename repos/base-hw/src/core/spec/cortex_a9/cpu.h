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

		/**
		 * Coprocessor Access Control Register
		 */
		struct Cpacr : Register<32>
		{
			struct Cp10 : Bitfield<20, 2> { };
			struct Cp11 : Bitfield<22, 2> { };

			/**
			 * Read register value
			 */
			static access_t read()
			{
				access_t v;
				asm volatile ("mrc p15, 0, %[v], c1, c0, 2" : [v]"=r"(v) ::);
				return v;
			}

			/**
			 * Override register value
			 *
			 * \param v  write value
			 */
			static void write(access_t const v) {
				asm volatile ("mcr p15, 0, %[v], c1, c0, 2" :: [v]"r"(v) :); }
		};

		struct User_context : Arm_cpu::User_context, Fpu::Context { };

		/**
		 * Next cpu context to switch to
		 *
		 * \param context  context to switch to
		 */
		void switch_to(User_context & context) { _fpu.switch_to(context); }

		/**
		 * Return wether to retry an undefined user instruction after this call
		 *
		 * \param state  CPU state of the user
		 */
		bool retry_undefined_instr(User_context & context) {
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
