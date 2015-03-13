/*
 * \brief  Class for kernel data that is needed to manage a specific CPU
 * \author Reto Buerki
 * \date   2015-02-09
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

using namespace Kernel;


Cpu_idle::Cpu_idle(Cpu * const cpu) : Cpu_job(Cpu_priority::min, 0)
{
	Cpu_job::cpu(cpu);
	ip = (addr_t)&_main;
	sp = (addr_t)&_stack[stack_size];
	init_thread((addr_t)core_pd()->translation_table(), core_pd()->id());
}


void Cpu_idle::exception(unsigned const cpu)
{
	if (trapno == RESET) {
		return;
	} else if (trapno >= INTERRUPTS_START && trapno <= INTERRUPTS_END) {
		_interrupt(cpu);
		return;
	}
	assert(0);
}
