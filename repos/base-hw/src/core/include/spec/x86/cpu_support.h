/*
 * \brief  x86 CPU driver for core
 * \author Adrian-Ken Rueegsegger
 * \author Martin stein
 * \author Reto Buerki
 * \date   2015-02-06
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SPEC__X86__CPU_SUPPORT_H_
#define _SPEC__X86__CPU_SUPPORT_H_

/* Genode includes */
#include <util/register.h>
#include <kernel/interface_support.h>
#include <cpu/cpu_state.h>

/* base includes */
#include <unmanaged_singleton.h>

/* core includes */
#include <gdt.h>
#include <idt.h>
#include <tss.h>
#include <timer.h>

extern int _mt_idt;
extern int _mt_tss;

namespace Genode
{
	/**
	 * Part of CPU state that is not switched on every mode transition
	 */
	class Cpu_lazy_state;

	/**
	 * CPU driver for core
	 */
	class Cpu;
}

namespace Kernel
{
	using Genode::Cpu_lazy_state;

	class Pd;
}

class Genode::Cpu_lazy_state
{
	friend class Cpu;

	private:

		/**
		 * FXSAVE area providing storage for x87 FPU, MMX, XMM, and MXCSR
		 * registers.
		 *
		 * For further details see Intel SDM Vol. 2A, 'FXSAVE instruction'.
		 */
		char fxsave_area[527];

		/**
		 * 16-byte aligned start of FXSAVE area.
		 */
		char *start;

		/**
		 * Load x87 FPU State from fxsave area.
		 */
		inline void load() { asm volatile ("fxrstor %0" : : "m" (*start)); }

		/**
		 * Save x87 FPU State to fxsave area.
		 */
		inline void save() { asm volatile ("fxsave %0" : "=m" (*start)); }

	public:

		/**
		 * Constructor
		 *
		 * Calculate 16-byte aligned start of FXSAVE area if necessary.
		 */
		inline Cpu_lazy_state()
		{
			start = fxsave_area;
			if((addr_t)start & 15)
				start = (char *)((addr_t)start & ~15) + 16;
		};
} __attribute__((aligned(16)));


class Genode::Cpu
{
	friend class Cpu_lazy_state;

	private:

		Idt *_idt;
		Tss *_tss;
		Cpu_lazy_state *_fpu_state;

		/**
		 * Control register 0
		 */
		struct Cr0 : Register<64>
		{
			struct Pe : Bitfield<0,  1> { };  /* Protection Enable   */
			struct Mp : Bitfield<1,  1> { };  /* Monitor Coprocessor */
			struct Em : Bitfield<2,  1> { };  /* Emulation           */
			struct Ts : Bitfield<3,  1> { };  /* Task Switched       */
			struct Et : Bitfield<4,  1> { };  /* Extension Type      */
			struct Ne : Bitfield<5,  1> { };  /* Numeric Error       */
			struct Wp : Bitfield<16, 1> { };  /* Write Protect       */
			struct Am : Bitfield<18, 1> { };  /* Alignment Mask      */
			struct Nw : Bitfield<29, 1> { };  /* Not Write-through   */
			struct Cd : Bitfield<30, 1> { };  /* Cache Disable       */
			struct Pg : Bitfield<31, 1> { };  /* Paging              */

			static void write(access_t const v) {
				asm volatile ("mov %0, %%cr0" :: "r" (v) : ); }

			static access_t read()
			{
				access_t v;
				asm volatile ("mov %%cr0, %0" : "=r" (v) :: );
				return v;
			}
		};

		/**
		 * Disable FPU by setting the TS flag in CR0.
		 */
		static void _disable_fpu()
		{
			Cr0::write(Cr0::read() | Cr0::Ts::bits(1));
		}

		/**
		 * Enable FPU by clearing the TS flag in CR0.
		 */
		static void _enable_fpu() { asm volatile ("clts"); }

		/**
		 * Initialize FPU without checking for pending unmasked floating-point
		 * exceptions.
		 */
		static void _init_fpu() { asm volatile ("fninit"); }

		/**
		 * Returns True if the FPU is enabled.
		 */
		static bool is_fpu_enabled() { return !Cr0::Ts::get(Cr0::read()); }

	public:

		Cpu() : _fpu_state(0)
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
			 * Initialize context
			 *
			 * \param table  physical base of appropriate translation table
			 * \param core   whether it is a core thread or not
			 */
			void init(addr_t const table, bool core)
			{
				/* Constants to handle IF, IOPL values */
				enum {
					EFLAGS_IF_SET = 1 << 9,
					EFLAGS_IOPL_3 = 3 << 12,
				};

				cr3 = Cr3::init(table);

				/*
				 * Enable interrupts for all threads, set I/O privilege level
				 * (IOPL) to 3 for core threads to allow UART access.
				 */
				eflags = EFLAGS_IF_SET;
				if (core) eflags |= EFLAGS_IOPL_3;
				else Gdt::load(Cpu::exception_entry);
			}
		};

		struct Pd {};

		/**
		 * An usermode execution state
		 */
		struct User_context : Context
		{
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
		flush_data_caches_by_virt_region(addr_t base, size_t const size) { }

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
		 * \param pd  kernel's pd object
		 */
		static void init_virt_kernel(Kernel::Pd * pd);

		inline static void finish_init_phys_kernel() { _init_fpu(); }

		/**
		 * Configure this module appropriately for the first kernel run
		 */
		static void init_phys_kernel() { Timer::disable_pit(); };

		/**
		 * Finish all previous data transfers
		 */
		static void data_synchronization_barrier() { }

		/**
		 * Enable secondary CPUs with instr. pointer 'ip'
		 */
		static void start_secondary_cpus(void * const ip) { }

		/**
		 * Wait for the next interrupt as cheap as possible
		 */
		static void wait_for_interrupt() { }

		/**
		 * Return wether to retry an undefined user instruction after this call
		 */
		bool retry_undefined_instr(Cpu_lazy_state *) { return false; }

		/**
		 * Return whether to retry an FPU instruction after this call
		 */
		bool retry_fpu_instr(Cpu_lazy_state * const state)
		{
			if (is_fpu_enabled())
				return false;

			_enable_fpu();
			if (_fpu_state != state) {
				if (_fpu_state)
					_fpu_state->save();

				state->load();
				_fpu_state = state;
			}
			return true;
		}

		/**
		 * Return kernel name of the executing CPU
		 */
		static unsigned executing_id() { return 0; }

		/**
		 * Return kernel name of the primary CPU
		 */
		static unsigned primary_id() { return 0; }

		/**
		 * Prepare for the proceeding of a user
		 *
		 * \param old_state  CPU state of the last user
		 * \param new_state  CPU state of the next user
		 */
		static void prepare_proceeding(Cpu_lazy_state * const old_state,
		                               Cpu_lazy_state * const new_state)
		{
			if (old_state == new_state)
				return;

			_disable_fpu();
		}

		/*************
		 ** Dummies **
		 *************/

		static void tlb_insertions() { inval_branch_predicts(); }
		static void translation_added(addr_t, size_t) { }

};

#endif /* _SPEC__X86__CPU_SUPPORT_H_ */
