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

/* core includes */
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/pd.h>


void Kernel::Cpu::init(Pic &pic)
{
	gdt.init((addr_t)&tss);
	Idt::init();
	Tss::init();

	Timer::init_cpu_local();

	fpu().init();

	/* enable timer interrupt */
	unsigned const cpu = Cpu::executing_id();
	pic.unmask(_timer.interrupt_id(), cpu);
}


void Kernel::Cpu_domain_update::_domain_update() { }
