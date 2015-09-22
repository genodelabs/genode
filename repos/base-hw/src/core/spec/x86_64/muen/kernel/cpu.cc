/*
 * \brief  Class for kernel data that is needed to manage a specific CPU
 * \author Reto Buerki
 * \date   2015-04-28
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/pd.h>

using namespace Kernel;


Cpu_idle::Cpu_idle(Cpu * const cpu) : Cpu_job(Cpu_priority::MIN, 0)
{
	Cpu_job::cpu(cpu);
	ip = (addr_t)&_main;
	sp = (addr_t)&_stack[stack_size];
	init((addr_t)core_pd()->translation_table(), true);
}


void Cpu_idle::exception(unsigned const cpu)
{
	if (trapno == RESET) return;

	if (trapno >= INTERRUPTS_START && trapno <= INTERRUPTS_END) {
		pic()->irq_occurred(trapno);
		_interrupt(cpu);
		return;
	}

	PWRN("Unknown exception %lu with error code %lu at ip=%p", trapno,
	     errcode, (void *)ip);
	assert(0);
}
