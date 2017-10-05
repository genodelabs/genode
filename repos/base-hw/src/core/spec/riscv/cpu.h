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

		/**
		 * Extend basic CPU state by members relevant for 'base-hw' only
		 */
		struct Context : Cpu_state
		{
			Sptbr::access_t sptbr;

			/**
			 * Return base of assigned translation table
			 */
			addr_t translation_table() const {
				return Sptbr::Ppn::get(sptbr) << 12; }

			/**
			 * Assign translation-table base 'table'
			 */
			void translation_table(addr_t const table) {
				Sptbr::Ppn::set(sptbr, table >> 12); }

			/**
			 * Assign protection domain
			 */
			void protection_domain(Genode::uint8_t const id) {
				Sptbr::Asid::set(sptbr, id); }
		};

		struct Pd
		{
			Genode::uint8_t asid; /* address space id */

			Pd(Genode::uint8_t id) : asid(id) {}
		};

		/**
		 * A usermode execution state
		 */
		struct User_context
		{
			Align_at<Context, 8> regs;

			User_context()
			{
				/*
				 * initialize cpu_exception with something that gets ignored in
				 * Thread::exception
				 */
				regs->cpu_exception = IRQ_FLAG;
			}

			/**
			 * Support for kernel calls
			 */
			void user_arg_0(Kernel::Call_arg const arg) { regs->a0  = arg; }
			void user_arg_1(Kernel::Call_arg const arg) { regs->a1  = arg; }
			void user_arg_2(Kernel::Call_arg const arg) { regs->a2  = arg; }
			void user_arg_3(Kernel::Call_arg const arg) { regs->a3  = arg; }
			void user_arg_4(Kernel::Call_arg const arg) { regs->a4  = arg; }
			Kernel::Call_arg user_arg_0() const { return regs->a0; }
			Kernel::Call_arg user_arg_1() const { return regs->a1; }
			Kernel::Call_arg user_arg_2() const { return regs->a2; }
			Kernel::Call_arg user_arg_3() const { return regs->a3; }
			Kernel::Call_arg user_arg_4() const { return regs->a4; }
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

		/**
		 * Post processing after a translation was added to a translation table
		 *
		 * \param addr  virtual address of the translation
		 * \param size  size of the translation
		 */
		static void translation_added(addr_t const addr, size_t const size);

		static void invalidate_tlb_by_pid(unsigned const pid) { sfence(); }

		/**
		 * Return kernel name of the executing CPU
		 */
		static unsigned executing_id() { return primary_id(); }

		/**
		 * Return kernel name of the primary CPU
		 */
		static unsigned primary_id() { return 0; }

		/*************
		 ** Dummies **
		 *************/

		void switch_to(User_context& context)
		{
			bool user = Sptbr::Asid::get(context.regs->sptbr);
			Sstatus::access_t v = Sstatus::read();
			Sstatus::Spp::set(v, user ? 0 : 1);
			Sstatus::write(v);
			if (user) Sptbr::write(context.regs->sptbr);
		}
};

#endif /* _CORE__SPEC__RISCV__CPU_H_ */
