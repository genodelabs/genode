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
		_interrupt(cpu);
		return;
	}

	PWRN("Unknown exception %lu with error code %lu at ip=%p", trapno,
	     errcode, (void *)ip);
	assert(0);
}
