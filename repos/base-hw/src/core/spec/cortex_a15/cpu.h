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

#ifndef _CORE__SPEC__CORTEX_A15__CPU_H_
#define _CORE__SPEC__CORTEX_A15__CPU_H_

/* core includes */
#include <spec/arm_v7/cpu_support.h>

namespace Genode { class Cpu; }


class Genode::Cpu : public Arm_v7_cpu
{
	public:

		/*********************************
		 **  Virtualization extensions  **
		 *********************************/

		/**
		 * Hypervisor system trap register
		 */
		struct Hstr : Register<32>
		{
			/* System coprocessor primary register access trap */
			template <unsigned R>
			struct T : Bitfield<R, 1> {};

			static access_t init()
			{
				/*
				 * allow cache (7), TLB (8) maintenance, and performance
				 * monitor (9), process/thread ID register (13) and timer (14)
				 * access.
				 */
				access_t v = 0;
				T<0>::set(v, 1);
				T<1>::set(v, 1);
				T<2>::set(v, 1);
				T<3>::set(v, 1);
				T<5>::set(v, 1);
				T<6>::set(v, 1);
				T<10>::set(v, 1);
				T<11>::set(v, 1);
				T<12>::set(v, 1);
				T<15>::set(v, 1);
				return v;
			};
		};

		/**
		 * Hypervisor control register
		 */
		struct Hcr : Register<32>
		{
			struct Vm    : Bitfield<0, 1>  {}; /* VT MMU enabled            */
			struct Fmo   : Bitfield<3, 1>  {}; /* FIQ cannot been masked    */
			struct Imo   : Bitfield<4, 1>  {}; /* IRQ cannot been masked    */
			struct Amo   : Bitfield<5, 1>  {}; /* A bit cannot been masked  */
			struct Twi   : Bitfield<13, 1> {}; /* trap on WFI instruction   */
			struct Twe   : Bitfield<14, 1> {}; /* trap on WFE instruction   */
			struct Tidcp : Bitfield<20, 1> {}; /* trap lockdown             */
			struct Tac   : Bitfield<21, 1> {}; /* trap ACTLR accesses       */
			struct Tvm   : Bitfield<26, 1> {}; /* trap virtual memory ctrls */

			static access_t init()
			{
				access_t v = 0;
				Vm::set(v, 1);
				Fmo::set(v, 1);
				Imo::set(v, 1);
				Amo::set(v, 1);
				Twi::set(v, 1);
				Twe::set(v, 1);
				Tidcp::set(v, 1);
				Tac::set(v, 1);
				Tvm::set(v, 1);
				return v;
			};
		};


		/**
		 * An usermode execution state
		 */
		struct Mmu_context
		{
			Ttbr_64bit::access_t ttbr0;

			Mmu_context(addr_t const table);
			~Mmu_context();

			Genode::uint8_t id() const { return Ttbr_64bit::Asid::get(ttbr0); }
		};


		/**
		 * Return if the context is in a page fault due to translation miss
		 *
		 * \param va  holds the virtual fault-address if call returns 1
		 * \param w   holds wether it's a write fault if call returns 1
		 * \param p   holds whether it's a permission fault if call returns 1
		 */
		static bool in_fault(Context & c, addr_t & va, addr_t & w, bool & p)
		{
			/* permission fault on page, 2nd level */
			static constexpr Fsr::access_t permission = 0b1111;

			switch (c.cpu_exception) {

			case Context::PREFETCH_ABORT:
				{
					/* check if fault was caused by a translation miss */
					Fsr::access_t const fs = Fsr::Fs::get(Ifsr::read());
					if (fs == permission) {
						w = 0;
						va = Ifar::read();
						p = true;
						return true;
					}

					if ((fs & 0b11100) != 0b100) return false;

					/* fetch fault data */
					w = 0;
					va = Ifar::read();
					p = false;
					return true;
				}

			case Context::DATA_ABORT:
				{
					/* check if fault was caused by translation miss */
					Fsr::access_t const fs = Fsr::Fs::get(Dfsr::read());
					if ((fs != permission) && (fs & 0b11100) != 0b100)
						return false;

					/* fetch fault data */
					Dfsr::access_t const dfsr = Dfsr::read();
					w = Dfsr::Wnr::get(dfsr);
					va = Dfar::read();
					p = false;
					return true;
				}

			default:
				return false;
			};
		};


		/**
		 * Return kernel name of the executing CPU
		 */
		static unsigned executing_id() { return Mpidr::Aff_0::get(Mpidr::read()); }

		/**
		 * Return kernel name of the primary CPU
		 */
		static unsigned primary_id();

		/**
		 * Write back dirty cache lines and invalidate all cache lines
		 */
		void clean_invalidate_data_cache() {
			clean_invalidate_inner_data_cache(); }

		/**
		 * Invalidate all cache lines
		 */
		void invalidate_data_cache() {
			invalidate_inner_data_cache(); }

		void switch_to(Context &, Mmu_context & mmu_context)
		{
			if (mmu_context.id() && (Ttbr0_64bit::read() != mmu_context.ttbr0))
				Ttbr0_64bit::write(mmu_context.ttbr0);
		}


		/*************
		 ** Dummies **
		 *************/

		bool retry_undefined_instr(Context&) { return false; }
};

#endif /* _CORE__SPEC__CORTEX_A15__CPU_H_ */
