/*
 * \brief  x86_64 CPU driver for core
 * \author Adrian-Ken Rueegsegger
 * \author Martin stein
 * \author Reto Buerki
 * \author Stefan Kalkowski
 * \date   2015-02-06
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SPEC__X86_64__CPU_H_
#define _CORE__INCLUDE__SPEC__X86_64__CPU_H_

/* Genode includes */
#include <util/register.h>
#include <kernel/interface_support.h>
#include <cpu/cpu_state.h>

/* base includes */
#include <base/internal/unmanaged_singleton.h>

/* core includes */
#include <fpu.h>

namespace Genode {
	class Cpu;
	using sizet_arithm_t = __uint128_t;
}


class Genode::Cpu
{
	public:

		struct Pd {};

		struct Pseudo_descriptor;

		struct Cr0; /* Control register 0         */
		struct Cr2; /* Control register 2         */
		struct Cr3; /* Control register 3         */
		struct Cr4; /* Control register 4         */


		/**
		 * Task State Segment (TSS)
		 *
		 * See Intel SDM Vol. 3A, section 7.7
		 */
		struct Tss { static void init(); };


		/**
		 * Interrupt Descriptor Table (IDT)
		 *
		 * See Intel SDM Vol. 3A, section 6.10
		 */
		struct Idt { static void init(); };


		/**
		 * Global Descriptor Table (GDT)
		 * See Intel SDM Vol. 3A, section 3.5.1
		 */
		struct Gdt { static void init(); };


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
			void init(addr_t const table, bool core);
		};


		/**
		 * An usermode execution state
		 */
		struct User_context : Context, Fpu::Context
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

	protected:

		Fpu _fpu;

	public:

		Fpu & fpu() { return _fpu; }

		static constexpr addr_t exception_entry = 0xffff0000;
		static constexpr addr_t mtc_size        = 1 << 13;

		static addr_t virt_mtc_addr(addr_t virt_base, addr_t label);

		/**
		 * Wait for the next interrupt as cheap as possible
		 */
		static void wait_for_interrupt() { asm volatile ("pause"); }

		/**
		 * Return wether to retry an undefined user instruction after this call
		 */
		bool retry_undefined_instr(Context&) { return false; }


		/**
		 * Return kernel name of the executing CPU
		 */
		static unsigned executing_id() { return 0; }

		/**
		 * Return kernel name of the primary CPU
		 */
		static unsigned primary_id() { return 0; }

		/**
		 * Switch to new context
		 *
		 * \param context  next CPU context
		 */
		void switch_to(User_context &context) { _fpu.switch_to(context); }
};


/**
 * Pseudo Descriptor
 *
 * See Intel SDM Vol. 3A, section 3.5.1
 */
struct Genode::Cpu::Pseudo_descriptor
{
	uint16_t const limit = 0;
	uint64_t const base  = 0;

	constexpr Pseudo_descriptor(uint16_t l, uint64_t b) : limit(l), base(b) {}
} __attribute__((packed));


struct Genode::Cpu::Cr0 : Register<64>
{
	struct Pe : Bitfield< 0, 1> { }; /* Protection Enable   */
	struct Mp : Bitfield< 1, 1> { }; /* Monitor Coprocessor */
	struct Em : Bitfield< 2, 1> { }; /* Emulation           */
	struct Ts : Bitfield< 3, 1> { }; /* Task Switched       */
	struct Et : Bitfield< 4, 1> { }; /* Extension Type      */
	struct Ne : Bitfield< 5, 1> { }; /* Numeric Error       */
	struct Wp : Bitfield<16, 1> { }; /* Write Protect       */
	struct Am : Bitfield<18, 1> { }; /* Alignment Mask      */
	struct Nw : Bitfield<29, 1> { }; /* Not Write-through   */
	struct Cd : Bitfield<30, 1> { }; /* Cache Disable       */
	struct Pg : Bitfield<31, 1> { }; /* Paging              */

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
 * Control register 2: Page-fault linear address
 *
 * See Intel SDM Vol. 3A, section 2.5.
 */
struct Genode::Cpu::Cr2 : Register<64>
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
struct Genode::Cpu::Cr3 : Register<64>
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


struct Genode::Cpu::Cr4 : Register<64>
{
	struct Vme        : Bitfield< 0, 1> { }; /* Virtual-8086 Mode Extensions */
	struct Pvi        : Bitfield< 1, 1> { }; /* Protected-Mode Virtual IRQs */
	struct Tsd        : Bitfield< 2, 1> { }; /* Time Stamp Disable */
	struct De         : Bitfield< 3, 1> { }; /* Debugging Exceptions */
	struct Pse        : Bitfield< 4, 1> { }; /* Page Size Extensions */
	struct Pae        : Bitfield< 5, 1> { }; /* Physical Address Extension */
	struct Mce        : Bitfield< 6, 1> { }; /* Machine-Check Enable */
	struct Pge        : Bitfield< 7, 1> { }; /* Page Global Enable */
	struct Pce        : Bitfield< 8, 1> { }; /* Performance-Monitoring Counter
	                                            Enable*/
	struct Osfxsr     : Bitfield< 9, 1> { }; /* OS Support for FXSAVE and
	                                            FXRSTOR instructions*/
	struct Osxmmexcpt : Bitfield<10, 1> { }; /* OS Support for Unmasked
	                                            SIMD/FPU Exceptions */
	struct Vmxe       : Bitfield<13, 1> { }; /* VMX Enable */
	struct Smxe       : Bitfield<14, 1> { }; /* SMX Enable */
	struct Fsgsbase   : Bitfield<16, 1> { }; /* FSGSBASE-Enable */
	struct Pcide      : Bitfield<17, 1> { }; /* PCIDE Enable */
	struct Osxsave    : Bitfield<18, 1> { }; /* XSAVE and Processor Extended
	                                            States-Enable */
	struct Smep       : Bitfield<20, 1> { }; /* SMEP Enable */
	struct Smap       : Bitfield<21, 1> { }; /* SMAP Enable */

	static void write(access_t const v) {
		asm volatile ("mov %0, %%cr4" :: "r" (v) : ); }

	static access_t read()
	{
		access_t v;
		asm volatile ("mov %%cr4, %0" : "=r" (v) :: );
		return v;
	}
};

#endif /* _CORE__INCLUDE__SPEC__X86_64__CPU_H_ */
