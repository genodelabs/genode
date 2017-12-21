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

#ifndef _CORE__SPEC__X86_64__CPU_H_
#define _CORE__SPEC__X86_64__CPU_H_

/* Genode includes */
#include <util/register.h>
#include <kernel/interface_support.h>
#include <cpu/cpu_state.h>

#include <hw/spec/x86_64/cpu.h>

/* base includes */
#include <base/internal/align_at.h>
#include <base/internal/unmanaged_singleton.h>

/* core includes */
#include <fpu.h>

namespace Kernel { struct Thread_fault; }

namespace Genode {
	class Cpu;
	using sizet_arithm_t = __uint128_t;
}


class Genode::Cpu : public Hw::X86_64_cpu
{
	protected:

		Fpu _fpu { };

	public:

		/**
		 * Task State Segment (TSS)
		 *
		 * See Intel SDM Vol. 3A, section 7.7
		 */
		struct alignas(8) Tss
		{
			uint32_t reserved0;
			uint64_t rsp[3];       /* pl0-3 stack pointer */
			uint64_t reserved1;
			uint64_t ist[7];       /* irq stack pointer   */
			uint64_t reserved2;

			static void init();
		}  __attribute__((packed)) tss { };


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
		struct alignas(8) Gdt
		{
			uint64_t null_desc         = 0;
			uint64_t sys_cs_64bit_desc = 0x20980000000000;
			uint64_t sys_ds_64bit_desc = 0x20930000000000;
			uint64_t usr_cs_64bit_desc = 0x20f80000000000;
			uint64_t usr_ds_64bit_desc = 0x20f30000000000;
			uint64_t tss_desc[2];

			void init(addr_t tss_addr);
		} __attribute__((packed)) gdt { };


		/**
		 * Extend basic CPU state by members relevant for 'base-hw' only
		 */
		struct alignas(16) Context : Cpu_state, Fpu::Context
		{
			enum Eflags {
				EFLAGS_IF_SET = 1 << 9,
				EFLAGS_IOPL_3 = 3 << 12,
			};

			Context(bool privileged);
		};


		struct Mmu_context
		{
			addr_t cr3;

			Mmu_context(addr_t page_table_base);
		};


		Fpu & fpu() { return _fpu; }

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
		void switch_to(Context & context, Mmu_context &mmu_context);

		static void mmu_fault(Context & regs, Kernel::Thread_fault & fault);
};

#endif /* _CORE__SPEC__X86_64__CPU_H_ */
