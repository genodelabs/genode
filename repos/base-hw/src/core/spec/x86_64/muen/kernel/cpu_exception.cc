/*
 * \brief   Kernel backend for protection domains
 * \author  Reto Buerki
 * \author  Stefan Kalkowski
 * \date    2016-01-11
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <assert.h>
#include <kernel/cpu.h>

void Kernel::Cpu_idle::exception(unsigned const cpu)
{
	if (trapno == RESET) return;

	if (trapno >= INTERRUPTS_START && trapno <= INTERRUPTS_END) {
		pic()->irq_occurred(trapno);
		_interrupt(cpu);
		return;
	}

	Genode::warning("Unknown exception ", trapno, " with error code ", errcode,
	                " at ip=", (void *)ip);
	assert(0);
}
