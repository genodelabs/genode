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


void Kernel::Cpu::_arch_init()
{
	gdt.init((addr_t)&tss);
	Idt::init();
	Tss::init();

	_pic.init();
	_timer.init();
	_ipi_irq.init();

	/* enable timer interrupt */
	_pic.unmask(_timer.interrupt_id(), id());
}
