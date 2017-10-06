/*
 * \brief   Kernel backend for protection domains
 * \author  Stefan Kalkowski
 * \date    2015-03-20
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <cpu.h>
#include <kernel/pd.h>

extern int __tss;
extern int __idt;
extern int __gdt_start;
extern int __gdt_end;


Genode::Cpu::Context::Context(bool core)
{
	eflags = EFLAGS_IF_SET;
	cs     = core ? 0x8 : 0x1b;
	ss     = core ? 0x10 : 0x23;
}


Genode::Cpu::Mmu_context::Mmu_context(addr_t const table)
: cr3(Cr3::Pdb::masked(table)) {}


void Genode::Cpu::Tss::init()
{
	enum { TSS_SELECTOR = 0x28, };
	asm volatile ("ltr %w0" : : "r" (TSS_SELECTOR));
}


void Genode::Cpu::Idt::init()
{
	Pseudo_descriptor descriptor {
		(uint16_t)((addr_t)&__tss - (addr_t)&__idt),
		(uint64_t)(&__idt) };
	asm volatile ("lidt %0" : : "m" (descriptor));
}


void Genode::Cpu::Gdt::init()
{
	addr_t const   start = (addr_t)&__gdt_start;
	uint16_t const limit = __gdt_end - __gdt_start - 1;
	uint64_t const base  = start;
	asm volatile ("lgdt %0" :: "m" (Pseudo_descriptor(limit, base)));
}
