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

extern int _mt_begin;
extern int _mt_tss;
extern int _mt_idt;
extern int _mt_gdt_start;
extern int _mt_gdt_end;


Genode::addr_t Genode::Cpu::virt_mtc_addr(Genode::addr_t virt_base,
                                          Genode::addr_t label) {
	return virt_base + (label - (addr_t)&_mt_begin); }


void Genode::Cpu::Context::init(addr_t const table, bool core)
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
}


void Genode::Cpu::Tss::init()
{
	enum { TSS_SELECTOR = 0x28, };
	asm volatile ("ltr %w0" : : "r" (TSS_SELECTOR));
}


void Genode::Cpu::Idt::init()
{
	Pseudo_descriptor descriptor {
		(uint16_t)((addr_t)&_mt_tss - (addr_t)&_mt_idt),
		(uint64_t)(virt_mtc_addr(exception_entry, (addr_t)&_mt_idt)) };
	asm volatile ("lidt %0" : : "m" (descriptor));
}


void Genode::Cpu::Gdt::init()
{
	addr_t const   start = (addr_t)&_mt_gdt_start;
	uint16_t const limit = _mt_gdt_end - _mt_gdt_start - 1;
	uint64_t const base  = virt_mtc_addr(exception_entry, start);
	asm volatile ("lgdt %0" :: "m" (Pseudo_descriptor(limit, base)));
}
