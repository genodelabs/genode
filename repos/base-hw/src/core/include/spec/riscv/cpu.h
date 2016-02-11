/*
 * \brief  CPU driver for core
 * \author Sebastian Sumpf
 * \date   2015-06-02
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CPU_H_
#define _CPU_H_

/* Genode includes */
#include <base/stdint.h>
#include <cpu/cpu_state.h>

namespace Genode
{
	/**
	 * CPU driver for core
	 */
	class Cpu;

	typedef __uint128_t sizet_arithm_t;
}

namespace Kernel { class Pd; }

class Genode::Cpu
{
	public:

		static constexpr addr_t mtc_size = 0x1000;
		static constexpr addr_t exception_entry = (~0ULL) & ~(0xfff);

		/**
		 * Extend basic CPU state by members relevant for 'base-hw' only
		 */
		struct Context : Cpu_state
		{
			addr_t sasid = 0;
			addr_t sptbr = 0; /* supervisor page table register */

			/**
			 * Return base of assigned translation table
			 */
			addr_t translation_table() const { return sptbr; }

			/**
			 * Assign translation-table base 'table'
			 */
			void translation_table(addr_t const table) { sptbr = table; }

			/**
			 * Assign protection domain
			 */
			void protection_domain(Genode::uint8_t const id) { sasid = id; }
		};

		struct Pd
		{
			Genode::uint8_t asid; /* address space id */

			Pd(Genode::uint8_t id) : asid(id) {}
		};

		/**
		 * A usermode execution state
		 */
		struct User_context : Context
		{
			/**
			 * Constructor
			 */
			User_context();

			/**
			 * Support for kernel calls
			 */
			void user_arg_0(unsigned const arg) { a0  = arg; }
			void user_arg_1(unsigned const arg) { a1  = arg; }
			void user_arg_2(unsigned const arg) { a2  = arg; }
			void user_arg_3(unsigned const arg) { a3  = arg; }
			void user_arg_4(unsigned const arg) { a4  = arg; }
			addr_t user_arg_0() const { return a0; }
			addr_t user_arg_1() const { return a1; }
			addr_t user_arg_2() const { return a2; }
			addr_t user_arg_3() const { return a3; }
			addr_t user_arg_4() const { return a4; }

			/**
			 * Initialize thread context
			 *
			 * \param table  physical base of appropriate translation table
			 * \param pd_id  kernel name of appropriate protection domain
			 */
			void init_thread(addr_t const table, unsigned const pd_id)
			{
				protection_domain(pd_id);
				translation_table(table);
			}
		};

		static void wait_for_interrupt() { asm volatile ("wfi"); };

		/**
		 * Post processing after a translation was added to a translation table
		 *
		 * \param addr  virtual address of the translation
		 * \param size  size of the translation
		 */
		static void translation_added(addr_t const addr, size_t const size)
		{
			PDBG("not impl");
		}

		/**
		 * Return kernel name of the executing CPU
		 */
		static unsigned executing_id() { return primary_id(); }

		/**
		 * Return kernel name of the primary CPU
		 */
		static unsigned primary_id() { return 0; }

		static void flush_tlb_by_pid(unsigned const pid)
		{
			PDBG("not impl");
		}

		static addr_t sbadaddr()
		{
			addr_t addr;
			asm volatile ("csrr %0, sbadaddr\n" : "=r"(addr));
			return addr;
		}

		static void data_synchronization_barrier() {
			asm volatile ("fence\n" : : : "memory"); }

		/*************
		 ** Dummies **
		 *************/

		void switch_to(User_context&) { }
		static void prepare_proceeding(Cpu_lazy_state *, Cpu_lazy_state *) { }
		static void invalidate_instr_caches() { }
		static void invalidate_data_caches() { }
		static void flush_data_caches() { }
		static void flush_data_caches_by_virt_region(addr_t, size_t) { }
		static void invalidate_instr_caches_by_virt_region(addr_t, size_t) { }
};

#endif /* _CPU_H_ */
