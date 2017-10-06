/*
 * \brief  Kernel entrypoint for SMP systems
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2011-10-20
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/cpu.h>
#include <kernel/lock.h>


extern "C" void kernel()
{
	using namespace Kernel;

	Cpu_job * new_job;
	Cpu     * cpu;

	{
		Lock::Guard guard(data_lock());

		cpu = cpu_pool()->cpu(Cpu::executing_id());
		new_job = &cpu->schedule();
	}

	new_job->proceed(*cpu);
}
