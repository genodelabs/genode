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

		/**
		 * Translation table base register 0 (64-bit format)
		 */
		struct Ttbr0 : Ttbr0_64bit
		{
			enum Memory_region { NON_CACHEABLE = 0, CACHEABLE = 1 };

			/**
			 * Return initialized value
			 *
			 * \param table  base of targeted translation table
			 */
			static access_t init(addr_t const table, unsigned const id)
			{
				access_t v = Ttbr_64bit::Ba::masked((access_t)table);
				Ttbr_64bit::Asid::set(v, id);
				return v;
			}

			static Genode::uint32_t init(addr_t const table) {
				return table; }
		};


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
		 * Extend basic CPU state by members relevant for 'base-hw' only
		 *
		 * Note: this class redefines Genode::Arm::Context
		 */
		struct Context : Genode::Cpu_state
		{
			Ttbr0::access_t ttbr0 = 0;
			addr_t sctlr = 0;
			addr_t ttbrc = 0;
			addr_t mair0 = 0;

			/**
			 * Return base of assigned translation table
			 */
			addr_t translation_table() const {
				return Ttbr_64bit::Ba::masked(ttbr0); }

			/**
			 * Assign translation-table base 'table'
			 */
			void translation_table(addr_t const table) {
				Ttbr_64bit::Ba::set(ttbr0, Ttbr_64bit::Ba::get(table)); }

			/**
			 * Assign protection domain
			 */
			void protection_domain(Genode::uint8_t const id) {
				Ttbr_64bit::Asid::set(ttbr0, id); }
		};


		/**
		 * An usermode execution state
		 *
		 * FIXME: this class largely overlaps with Genode::Arm::User_context
		 */
		struct User_context
		{
			Align_at<Context, 8> regs;

			void init(bool privileged)
			{
				Psr::access_t v = 0;
				Psr::M::set(v, privileged ? Psr::M::SYS : Psr::M::USR);
				Psr::F::set(v, 1);
				Psr::A::set(v, 1);
				regs->cpsr = v;
				regs->cpu_exception = Cpu::Context::RESET;
			}

			/**
			 * Support for kernel calls
			 */
			void user_arg_0(Kernel::Call_arg const arg) { regs->r0 = arg; }
			void user_arg_1(Kernel::Call_arg const arg) { regs->r1 = arg; }
			void user_arg_2(Kernel::Call_arg const arg) { regs->r2 = arg; }
			void user_arg_3(Kernel::Call_arg const arg) { regs->r3 = arg; }
			void user_arg_4(Kernel::Call_arg const arg) { regs->r4 = arg; }
			Kernel::Call_arg user_arg_0() const { return regs->r0; }
			Kernel::Call_arg user_arg_1() const { return regs->r1; }
			Kernel::Call_arg user_arg_2() const { return regs->r2; }
			Kernel::Call_arg user_arg_3() const { return regs->r3; }
			Kernel::Call_arg user_arg_4() const { return regs->r4; }

			/**
			 * Return if the context is in a page fault due to translation miss
			 *
			 * \param va  holds the virtual fault-address if call returns 1
			 * \param w   holds whether it's a write fault if call returns 1
			 * \param p   holds whether it's a permission fault if call returns 1
			 */
			bool in_fault(addr_t & va, addr_t & w, bool &p) const
			{
				/* permission fault on page, 2nd level */
				static constexpr Fsr::access_t permission = 0b1111;

				switch (regs->cpu_exception) {

				case Context::PREFETCH_ABORT:
					{
						/* check if fault was caused by a translation miss */
						Fsr::access_t const fs = Fsr::Fs::get(Ifsr::read());
						if (fs == permission) {
							w = 0;
							va = regs->ip;
							p = true;
							return true;
						}

						if ((fs & 0b11100) != 0b100) return false;

						/* fetch fault data */
						w = 0;
						va = regs->ip;
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
			}
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

		void switch_to(User_context& o)
		{
			if (Ttbr_64bit::Asid::get(o.regs->ttbr0) &&
			    (Ttbr0_64bit::read() != o.regs->ttbr0))
				Ttbr0_64bit::write(o.regs->ttbr0);
		}


		/*************
		 ** Dummies **
		 *************/

		bool retry_undefined_instr(User_context&) { return false; }
};

#endif /* _CORE__SPEC__CORTEX_A15__CPU_H_ */
