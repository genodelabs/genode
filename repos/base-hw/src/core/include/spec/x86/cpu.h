/*
 * \brief  CPU driver for core
 * \author Martin stein
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CPU_H_
#define _CPU_H_

#include <util/register.h>
#include <unmanaged_singleton.h>
#include <kernel/interface_support.h>
#include <cpu/cpu_state.h>
#include <gdt.h>
#include <idt.h>
#include <tss.h>

extern int _mt_idt;
extern int _mt_tss;

namespace Genode
{
	/**
	 * Part of CPU state that is not switched on every mode transition
	 */
	class Cpu_lazy_state { };

	/**
	 * CPU driver for core
	 */
	class Cpu;
}

namespace Kernel { using Genode::Cpu_lazy_state; }

class Genode::Cpu
{
	private:
		Idt *_idt;
		Tss *_tss;

	public:

		Cpu()
		{
			if (primary_id() == executing_id()) {
				_idt = new (&_mt_idt) Idt();
				_idt->setup(Cpu::exception_entry);

				_tss = new (&_mt_tss) Tss();
				_tss->load();
			}
			_idt->load(Cpu::exception_entry);
			_tss->setup(Cpu::exception_entry);
		}

		static constexpr addr_t exception_entry = 0xffff0000;
		static constexpr addr_t mtc_size        = 1 << 13;

		/**
		 * Control register 2: Page-fault linear address
		 *
		 * See Intel SDM Vol. 3A, section 2.5.
		 */
		struct Cr2 : Register<64>
		{
			struct Addr : Bitfield<0, 63> { };

			static access_t read()
			{
				access_t v;
				asm volatile ("mov %%cr2, %0" : "=r" (v) :: );
				return v;
			}
		};

		/**
		 * Control register 3: Page-Directory base register
		 *
		 * See Intel SDM Vol. 3A, section 2.5.
		 */
		struct Cr3 : Register<64>
		{
			struct Pwt : Bitfield<3,1> { };    /* Page-level write-through    */
			struct Pcd : Bitfield<4,1> { };    /* Page-level cache disable    */
			struct Pdb : Bitfield<12, 36> { }; /* Page-directory base address */

			static void write(access_t const v) {
				asm volatile ("mov %0, %%cr3" :: "r" (v) : ); }

			static access_t read()
			{
				access_t v;
				asm volatile ("mov %%cr3, %0" : "=r" (v) :: );
				return v;
			}

			/**
			 * Return initialized value
			 *
			 * \param table  base of targeted translation table
			 */
			static access_t init(addr_t const table) {
				return Pdb::masked(table); }
		};

		/**
		 * Extend basic CPU state by members relevant for 'base-hw' only
		 */
		struct Context : Cpu_state
		{
			/**
			 * Address of top-level paging structure.
			 */
			addr_t cr3;

			/**
			 * Return base of assigned translation table
			 */
			addr_t translation_table() const { return cr3; }

			/**
			 * Assign translation-table base 'table'
			 */
			void translation_table(addr_t const table) {
				cr3 = Cr3::init(table); }

			/**
			 * Assign protection domain
			 */
			void protection_domain(unsigned const id) { }
		};

		/**
		 * An usermode execution state
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
			void user_arg_0(Kernel::Call_arg const arg) { rdi = arg; }
			void user_arg_1(Kernel::Call_arg const arg) { rsi = arg; }
			void user_arg_2(Kernel::Call_arg const arg) { rdx = arg; }
			void user_arg_3(Kernel::Call_arg const arg) { rcx = arg; }
			void user_arg_4(Kernel::Call_arg const arg) { r8 = arg; }
			void user_arg_5(Kernel::Call_arg const arg) { r9 = arg; }
			void user_arg_6(Kernel::Call_arg const arg) { r10 = arg; }
			void user_arg_7(Kernel::Call_arg const arg) { r11 = arg; }
			Kernel::Call_arg user_arg_0() const { return rdi; }
			Kernel::Call_arg user_arg_1() const { return rsi; }
			Kernel::Call_arg user_arg_2() const { return rdx; }
			Kernel::Call_arg user_arg_3() const { return rcx; }
			Kernel::Call_arg user_arg_4() const { return r8; }
			Kernel::Call_arg user_arg_5() const { return r9; }
			Kernel::Call_arg user_arg_6() const { return r10; }
			Kernel::Call_arg user_arg_7() const { return r11; }

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

				Gdt::load(Cpu::exception_entry);
			}
		};

		/**
		 * Returns true if current execution context is running in user mode
		 */
		static bool is_user()
		{
			PDBG("not implemented");
			return false;
		}

		/**
		 * Invalidate all entries of all instruction caches
		 */
		__attribute__((always_inline)) static void invalidate_instr_caches() { }

		/**
		 * Flush all entries of all data caches
		 */
		inline static void flush_data_caches() { }

		/**
		 * Invalidate all entries of all data caches
		 */
		inline static void invalidate_data_caches() { }

		/**
		 * Flush all caches
		 */
		static void flush_caches()
		{
			flush_data_caches();
			invalidate_instr_caches();
		}

		/**
		 * Invalidate all TLB entries of the address space named 'pid'
		 */
		static void flush_tlb_by_pid(unsigned const pid)
		{
			flush_caches();
		}

		/**
		 * Invalidate all TLB entries
		 */
		static void flush_tlb()
		{
			flush_caches();
		}

		/**
		 * Flush data-cache entries for virtual region ['base', 'base + size')
		 */
		static void
		flush_data_caches_by_virt_region(addr_t base, size_t const size)
		{ }

		/**
		 * Bin instr.-cache entries for virtual region ['base', 'base + size')
		 */
		static void
		invalidate_instr_caches_by_virt_region(addr_t base, size_t const size)
		{ }

		static void inval_branch_predicts() { };

		/**
		 * Switch to the virtual mode in kernel
		 *
		 * \param table       base of targeted translation table
		 * \param process_id  process ID of the kernel address-space
		 */
		static void
		init_virt_kernel(addr_t const table, unsigned const process_id) {
			Cr3::write(Cr3::init(table)); }

		inline static void finish_init_phys_kernel()
		{ }

		/**
		 * Configure this module appropriately for the first kernel run
		 */
		static void init_phys_kernel()
		{ }

		/**
		 * Finish all previous data transfers
		 */
		static void data_synchronization_barrier()
		{ }

		/**
		 * Enable secondary CPUs with instr. pointer 'ip'
		 */
		static void start_secondary_cpus(void * const ip)
		{ }

		/**
		 * Wait for the next interrupt as cheap as possible
		 */
		static void wait_for_interrupt() { }

		/**
		 * Return wether to retry an undefined user instruction after this call
		 */
		bool retry_undefined_instr(Cpu_lazy_state *) { return false; }

		/**
		 * Return kernel name of the executing CPU
		 */
		static unsigned executing_id() { return 0; }

		/**
		 * Return kernel name of the primary CPU
		 */
		static unsigned primary_id() { return 0; }

		/*************
		 ** Dummies **
		 *************/

		static void tlb_insertions() { inval_branch_predicts(); }
		static void translation_added(addr_t, size_t) { }
		static void prepare_proceeding(Cpu_lazy_state *, Cpu_lazy_state *) { }

};

#endif /* _CPU_H_ */
