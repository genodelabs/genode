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

#ifndef _SPEC__CORTEX_A9__CPU_SUPPORT_H_
#define _SPEC__CORTEX_A9__CPU_SUPPORT_H_

/* core includes */
#include <spec/arm/fpu.h>
#include <spec/arm_v7/cpu_support.h>
#include <board.h>

namespace Genode { class Cortex_a9; }

class Genode::Cortex_a9 : public Arm_v7
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

		/**
		 * Auxiliary Control Register
		 */
		struct Actlr : Register<32>
		{
			struct Smp : Bitfield<6, 1> { };

			static access_t read()
			{
				access_t v;
				asm volatile ("mrc p15, 0, %0, c1, c0, 1" : "=r" (v) :: );
				return v;
			}

			static void write(access_t const v) {
				asm volatile ("mcr p15, 0, %0, c1, c0, 1" :: "r" (v) : ); }

			static void enable_smp(Board&)
			{
				access_t v = read();
				Smp::set(v, 1);
				write(v);
			}
		};

		struct User_context : Arm::User_context, Fpu::Context { };

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
			Kernel::board().l2_cache().clean_invalidate();
		}

		/**
		 * Invalidate whole data cache
		 */
		void invalidate_data_cache()
		{
			invalidate_inner_data_cache();
			Kernel::board().l2_cache().invalidate();
		}

		/**
		 * Clean and invalidate data-cache for virtual region
		 * 'base' - 'base + size'
		 */
		void clean_invalidate_data_cache_by_virt_region(addr_t base,
		                                                size_t const size)
		{
			Arm::clean_invalidate_data_cache_by_virt_region(base, size);
			Kernel::board().l2_cache().clean_invalidate();
		}

		void translation_table_insertions() { invalidate_branch_predicts(); }

		static unsigned executing_id() { return Mpidr::Aff_0::get(Mpidr::read()); }


		/*************
		 ** Dummies **
		 *************/

		static void translation_added(addr_t, size_t) { }
};

#endif /* _SPEC__CORTEX_A9__CPU_SUPPORT_H_ */
