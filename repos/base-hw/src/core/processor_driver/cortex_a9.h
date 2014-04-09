/*
 * \brief  Processor driver for core
 * \author Martin stein
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PROCESSOR_DRIVER__CORTEX_A9_H_
#define _PROCESSOR_DRIVER__CORTEX_A9_H_

/* core includes */
#include <processor_driver/arm_v7.h>
#include <board.h>

namespace Cortex_a9
{
	using namespace Genode;

	/**
	 * Part of processor state that is not switched on every mode transition
	 */
	class Processor_lazy_state;

	/**
	 * Processor driver for core
	 */
	class Processor_driver;
}

class Cortex_a9::Processor_lazy_state
{
	friend class Processor_driver;

	private:

		/* advanced FP/SIMD - system registers */
		uint32_t fpscr;
		uint32_t fpexc;

		/* advanced FP/SIMD - general purpose registers d0-d15 */
		uint64_t d0, d1, d2,  d3,  d4,  d5,  d6,  d7;
		uint64_t d8, d9, d10, d11, d12, d13, d14, d15;

	public:

		/**
		 * Constructor
		 */
		inline Processor_lazy_state();
};

class Cortex_a9::Processor_driver : public Arm_v7::Processor_driver
{
	friend class Processor_lazy_state;

	private:

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
			static void write(access_t const v)
			{
				asm volatile ("mcr p15, 0, %[v], c1, c0, 2" :: [v]"r"(v) :);
			}
		};

		/**
		 * Floating-point Status and Control Register
		 */
		struct Fpscr : Register<32>
		{
			/**
			 * Read register value
			 */
			static access_t read()
			{
				/* FIXME: See annotation 1. */
				access_t v;
				asm volatile ("mrc p10, 7, %[v], cr1, cr0, 0" : [v] "=r" (v) ::);
				return v;
			}

			/**
			 * Override register value
			 *
			 * \param v  write value
			 */
			static void write(access_t const v)
			{
				/* FIXME: See annotation 1. */
				asm volatile ("mcr p10, 7, %[v], cr1, cr0, 0" :: [v] "r" (v) :);
			}
		};

		/**
		 * Floating-Point Exception Control register
		 */
		struct Fpexc : Register<32>
		{
			struct En  : Bitfield<30, 1> { };

			/**
			 * Read register value
			 */
			static access_t read()
			{
				/* FIXME: See annotation 1. */
				access_t v;
				asm volatile ("mrc p10, 7, %[v], cr8, cr0, 0" : [v] "=r" (v) ::);
				return v;
			}

			/**
			 * Override register value
			 *
			 * \param v  write value
			 */
			static void write(access_t const v)
			{
				/* FIXME: See annotation 1. */
				asm volatile ("mcr p10, 7, %[v], cr8, cr0, 0" :: [v] "r" (v) :);
			}
		};

		Processor_lazy_state * _advanced_fp_simd_state;

		/**
		 * Enable or disable the advanced FP/SIMD extension
		 *
		 * \param enabled  wether to enable or to disable advanced FP/SIMD
		 */
		static void _toggle_advanced_fp_simd(bool const enabled)
		{
			Fpexc::access_t fpexc = Fpexc::read();
			Fpexc::En::set(fpexc, enabled);
			Fpexc::write(fpexc);
		}

		/**
		 * Save state of the advanced FP/SIMD extension to memory
		 *
		 * \param state  processor state to save FP/SIMD state into
		 */
		static void
		_save_advanced_fp_simd_state(Processor_lazy_state * const state)
		{
			/* save system registers */
			state->fpexc = Fpexc::read();
			state->fpscr = Fpscr::read();

			/*
			 * Save D0 - D15
			 *
			 * FIXME: See annotation 2.
			 */
			void * const d0_d15_base = &state->d0;
			asm volatile (
				"stc p11, cr0, [%[d0_d15_base]], #128"
				:: [d0_d15_base] "r" (d0_d15_base) : );
		}

		/**
		 * Load state of the advanced FP/SIMD extension from memory
		 *
		 * \param state  processor state to load FP/SIMD state out of
		 */
		static void
		_load_advanced_fp_simd_state(Processor_lazy_state * const state)
		{
			/* load system registers */
			Fpexc::write(state->fpexc);
			Fpscr::write(state->fpscr);

			/*
			 * Load D0 - D15
			 *
			 * FIXME: See annotation 2.
			 */
			void * const d0_d15_base = &state->d0;
			asm volatile (
				"ldc p11, cr0, [%[d0_d15_base]], #128"
				:: [d0_d15_base] "r" (d0_d15_base) : );
		}

		/**
		 * Return wether the advanced FP/SIMD extension is enabled
		 */
		static bool _advanced_fp_simd_enabled()
		{
			Fpexc::access_t fpexc = Fpexc::read();
			return Fpexc::En::get(fpexc);
		}

	public:

		enum
		{
			/* common */
			PERIPH_CLK = Board::CORTEX_A9_CLOCK,

			/* interrupt controller */
			PL390_DISTRIBUTOR_MMIO_BASE = Board::CORTEX_A9_PRIVATE_MEM_BASE + 0x1000,
			PL390_DISTRIBUTOR_MMIO_SIZE = 0x1000,
			PL390_CPU_MMIO_BASE = Board::CORTEX_A9_PRIVATE_MEM_BASE + 0x100,
			PL390_CPU_MMIO_SIZE = 0x100,

			/* timer */
			PRIVATE_TIMER_MMIO_BASE = Board::CORTEX_A9_PRIVATE_MEM_BASE + 0x600,
			PRIVATE_TIMER_MMIO_SIZE = 0x10,
			PRIVATE_TIMER_IRQ = 29,
			PRIVATE_TIMER_CLK = PERIPH_CLK
		};

		/**
		 * Constructor
		 */
		Processor_driver() : _advanced_fp_simd_state(0) { }

		/**
		 * Ensure that TLB insertions get applied
		 */
		static void tlb_insertions() { }

		/**
		 * Initialize advanced FP/SIMD extension
		 */
		static void init_advanced_fp_simd()
		{
			Cpacr::access_t cpacr = Cpacr::read();
			Cpacr::Cp10::set(cpacr, 3);
			Cpacr::Cp11::set(cpacr, 3);
			Cpacr::write(cpacr);
			_toggle_advanced_fp_simd(false);
		}

		/**
		 * Prepare for the proceeding of a user
		 *
		 * \param old_state  processor state of the last user
		 * \param new_state  processor state of the next user
		 */
		static void prepare_proceeding(Processor_lazy_state * const old_state,
		                               Processor_lazy_state * const new_state)
		{
			if (old_state == new_state) { return; }
			_toggle_advanced_fp_simd(false);
		}

		/**
		 * Return wether to retry an undefined user instruction after this call
		 *
		 * \param state  processor state of the user
		 */
		bool retry_undefined_instr(Processor_lazy_state * const state)
		{
			if (_advanced_fp_simd_enabled()) { return false; }
			_toggle_advanced_fp_simd(true);
			if (_advanced_fp_simd_state != state) {
				if (_advanced_fp_simd_state) {
					_save_advanced_fp_simd_state(_advanced_fp_simd_state);
				}
				_load_advanced_fp_simd_state(state);
				_advanced_fp_simd_state = state;
			}
			return true;
		}
};


/******************************
 ** Arm_v7::Processor_driver **
 ******************************/

void Arm_v7::Processor_driver::finish_init_phys_kernel()
{
	Cortex_a9::Processor_driver::init_advanced_fp_simd();
}


/*************************************
 ** Cortex_a9::Processor_lazy_state **
 *************************************/

Cortex_a9::Processor_lazy_state::Processor_lazy_state()
{
	fpexc = Processor_driver::Fpexc::En::bits(1);
}


/*****************
 ** Annotations **
 *****************/

/*
 * Annotation 1
 *
 *  According to the ARMv7 manual this should be done via vmsr/vmrs instruction
 *  but it seems that binutils 2.22 doesn't fully support this yet. Hence, we
 *  use a co-processor instruction instead. The parameters to target the
 *  register this way can be determined via 'sys/arm/include/vfp.h' and
 *  'sys/arm/arm/vfp.c' of the FreeBSD head branch as from 2014.04.17.
 */

/*
 * Annotation 2
 *
 *  According to the ARMv7 manual this should be done via vldm/vstm instruction
 *  but it seems that binutils 2.22 doesn't fully support this yet. Hence, we
 *  use a co-processor instruction instead. The parameters to target the
 *  register this way can be determined via 'sys/arm/arm/vfp.c' of the FreeBSD
 *  head branch as from 2014.04.17.
 */

#endif /* _PROCESSOR_DRIVER__CORTEX_A9_H_ */
