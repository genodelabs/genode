/*
 * \brief   Kernel backend for protection domains
 * \author  Stefan Kalkowski
 * \date    2015-03-20
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <cpu.h>
#include <kernel/pd.h>

Genode::Cpu::Cpu()
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
	else Gdt::load(Cpu::exception_entry);
}
