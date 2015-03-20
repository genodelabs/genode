/*
 * \brief  Interrupt Descriptor Table (IDT)
 * \author Adrian-Ken Rueegsegger
 * \author Reto Buerki
 * \date   2015-02-13
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <pseudo_descriptor.h>
#include <mtc_util.h>
#include <idt.h>

extern int _mt_idt;
extern int _mt_isrs;

using namespace Genode;


void Idt::setup(addr_t const virt_base)
{
	addr_t base = _virt_mtc_addr(virt_base, (addr_t)&_mt_isrs);
	addr_t isr_addr;

	for (unsigned vec = 0; vec < SIZE_IDT; vec++) {
		/* ISRs are padded to 4 bytes */
		isr_addr = base + vec * 0xc;

		_table[vec].offset_15_00 = isr_addr & 0xffff;
		_table[vec].segment_sel  = 8;
		_table[vec].flags        = 0x8e00;
		_table[vec].offset_31_16 = (isr_addr >> 16);
		_table[vec].offset_63_32 = (isr_addr >> 32);
	}

	/* Set DPL of syscall entry to 3 */
	_table[SYSCALL_VEC].flags = _table[SYSCALL_VEC].flags | 0x6000;
}


void Idt::load(addr_t const virt_base)
{
	uint16_t const limit = sizeof(_table) - 1;
	uint64_t const base  = _virt_mtc_addr(virt_base, (addr_t)&_mt_idt);
	asm volatile ("lidt %0" : : "m" (Pseudo_descriptor(limit, base)));
}
