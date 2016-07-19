/*
 * \brief  Muen subject state
 * \author Adrian-Ken Rueegsegger
 * \author Reto Buerki
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _VIRTUALBOX__SPEC__MUEN__VCPU_H_
#define _VIRTUALBOX__SPEC__MUEN__VCPU_H_

#include <base/stdint.h>

struct Cpu_registers
{
	Genode::uint64_t Cr2;
	Genode::uint64_t Rax;
	Genode::uint64_t Rbx;
	Genode::uint64_t Rcx;
	Genode::uint64_t Rdx;
	Genode::uint64_t Rdi;
	Genode::uint64_t Rsi;
	Genode::uint64_t Rbp;
	Genode::uint64_t R08;
	Genode::uint64_t R09;
	Genode::uint64_t R10;
	Genode::uint64_t R11;
	Genode::uint64_t R12;
	Genode::uint64_t R13;
	Genode::uint64_t R14;
	Genode::uint64_t R15;
} __attribute__((packed));

struct Segment
{
	Genode::uint64_t sel;
	Genode::uint64_t base;
	Genode::uint32_t limit;
	Genode::uint32_t access;
} __attribute__((packed));

struct Subject_state
{
	struct Cpu_registers Regs;
	Genode::uint32_t Exit_reason;
	Genode::uint32_t Intr_state;
	Genode::uint32_t Interrupt_info;
	Genode::uint32_t Sysenter_cs;
	Genode::uint64_t Exit_qualification;
	Genode::uint64_t Guest_phys_addr;
	Genode::uint64_t Instruction_len;
	Genode::uint64_t Rip;
	Genode::uint64_t Rsp;
	Genode::uint64_t Cr0;
	Genode::uint64_t Shadow_cr0;
	Genode::uint64_t Cr3;
	Genode::uint64_t Cr4;
	Genode::uint64_t Shadow_cr4;
	Genode::uint64_t Rflags;
	Genode::uint64_t Ia32_efer;
	Genode::uint64_t Sysenter_esp;
	Genode::uint64_t Sysenter_eip;
	Segment cs;
	Segment ss;
	Segment ds;
	Segment es;
	Segment fs;
	Segment gs;
	Segment tr;
	Segment ldtr;
	Segment gdtr;
	Segment idtr;
} __attribute__((packed));


/**
 * Print subject state information
 */
inline void dump_register_state(Subject_state * state)
{
	PINF("subject state");
	PLOG("ip:sp:efl ax:bx:cx:dx:si:di %lx:%lx:%lx %lx:%lx:%lx:%lx:%lx:%lx",
	     state->Rip, state->Rsp, state->Rflags,
	     state->Regs.Rax, state->Regs.Rbx, state->Regs.Rcx, state->Regs.Rdx,
		 state->Regs.Rsi, state->Regs.Rdi);

	PLOG("cs base:limit:sel:ar %lx:%x:%x:%x", state->cs.base,
	     state->cs.limit, state->cs.sel, state->cs.access);
	PLOG("ds base:limit:sel:ar %lx:%x:%x:%x", state->ds.base,
	     state->ds.limit, state->ds.sel, state->ds.access);
	PLOG("es base:limit:sel:ar %lx:%x:%x:%x", state->es.base,
	     state->es.limit, state->es.sel, state->es.access);
	PLOG("fs base:limit:sel:ar %lx:%x:%x:%x", state->fs.base,
	     state->fs.limit, state->fs.sel, state->fs.access);
	PLOG("gs base:limit:sel:ar %lx:%x:%x:%x", state->gs.base,
	     state->gs.limit, state->gs.sel, state->gs.access);
	PLOG("ss base:limit:sel:ar %lx:%x:%x:%x", state->ss.base,
	     state->ss.limit, state->ss.sel, state->ss.access);

	PLOG("cr0:cr2:cr3:cr4 %lx:%lx:%lx:%lx",
	     state->Cr0, state->Regs.Cr2, state->Cr3, state->Cr4);

	PLOG("ldtr base:limit:sel:ar %lx:%x:%x:%x", state->ldtr.base,
	     state->ldtr.limit, state->ldtr.sel, state->ldtr.access);
	PLOG("tr base:limit:sel:ar %lx:%x:%x:%x", state->tr.base,
	     state->tr.limit, state->tr.sel, state->tr.access);

	PLOG("gdtr base:limit %lx:%x", state->gdtr.base, state->gdtr.limit);
	PLOG("idtr base:limit %lx:%x", state->idtr.base, state->idtr.limit);

	PLOG("sysenter cs:eip:esp %lx %lx %lx", state->Sysenter_cs,
	     state->Sysenter_eip, state->Sysenter_esp);

	PLOG("%x", state->Intr_state);
}

#endif /* _VIRTUALBOX__SPEC__MUEN__VCPU_H_ */
