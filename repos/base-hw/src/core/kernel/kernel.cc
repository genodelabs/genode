/*
 * \brief  Kernel entrypoint for non-SMP systems
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2011-10-20
 */

/*
 * Copyright (C) 2011-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/cpu.h>


extern "C" void kernel()
{
	using namespace Kernel;

	Cpu * const cpu = cpu_pool()->cpu(Cpu::executing_id());
	cpu->schedule().proceed(cpu->id());
}


void Kernel::Cpu::Ipi::occurred() { }


void Kernel::Cpu::Ipi::trigger(unsigned const cpu_id) { }


Kernel::Cpu::Ipi::Ipi(Kernel::Irq::Pool &p) : Kernel::Irq(Kernel::Pic::IPI, p) { }
