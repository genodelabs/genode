/*
 * \brief   Class for kernel data that is needed to manage a specific CPU
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2014-01-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/cpu.h>
#include <kernel/pd.h>
#include <kernel/perf_counter.h>
#include <pic.h>
#include <trustzone.h>

void Kernel::Cpu::init(Kernel::Pic &pic, Kernel::Pd & core_pd)
{
	/* locally initialize interrupt controller */
	pic.init_cpu_local();

	/* initialize CPU in physical mode */
	Cpu::init_phys_kernel();

	/* switch to core address space */
	Cpu::init_virt_kernel(core_pd);

	/*
	 * TrustZone initialization code
	 *
	 * FIXME This is a plattform specific feature
	 */
	init_trustzone(pic);

	/*
	 * Enable performance counter
	 *
	 * FIXME This is an optional CPU specific feature
	 */
	perf_counter()->enable();

	/* enable timer interrupt */
	unsigned const cpu = Cpu::executing_id();
	pic.unmask(Timer::interrupt_id(cpu), cpu);
}

