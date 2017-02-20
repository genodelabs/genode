/*
 * \brief  Class for kernel data that is needed to manage a specific CPU
 * \author Reto Buerki
 * \author Stefan Kalkowski
 * \date   2015-02-09
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

/* core includes */
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/pd.h>

using namespace Kernel;


Cpu_idle::Cpu_idle(Cpu * const cpu) : Cpu_job(Cpu_priority::MIN, 0)
{
	Cpu::Gdt::init();
	Cpu_job::cpu(cpu);
	ip = (addr_t)&_main;
	sp = (addr_t)&_stack[stack_size];
	init((addr_t)core_pd()->translation_table(), true);
}


void Kernel::Cpu::init(Pic &pic)
{
	Idt::init();
	Tss::init();

	Timer::disable_pit();

	fpu().init();

	/* enable timer interrupt */
	unsigned const cpu = Cpu::executing_id();
	pic.unmask(Timer::interrupt_id(cpu), cpu);
}


void Cpu_domain_update::_domain_update() { }
