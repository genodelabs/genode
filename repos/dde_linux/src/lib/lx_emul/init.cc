/*
 * \brief  Lx_emul backend for Linux kernel initialization
 * \author Stefan Kalkowski
 * \date   2021-03-10
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/log.h>
#include <lx_kit/env.h>
#include <lx_kit/task.h>
#include <lx_emul/irq.h>
#include <lx_emul/init.h>
#include <lx_emul/task.h>

#include <lx_emul/initcall_order.h>

extern "C" void lx_emul_initcalls()
{
	Lx_kit::env().initcalls.execute_in_order();
}


extern "C" void lx_emul_register_initcall(int (*initcall)(void),
                                          const char * name)
{
	/*
	 * Filter out 'late_initcall_sync(clk_disable_unused)' in clk/clk.c to
	 * hold back a driver with access to the clock controller from affecting
	 * devices of other drivers.
	 */
	if (Genode::strcmp(name, "__initcall_clk_disable_unused7s") == 0)
		return;

	for (unsigned i = 0; i < (sizeof(lx_emul_initcall_order) / sizeof(char*));
	     i++) {
		if (Genode::strcmp(name, lx_emul_initcall_order[i]) == 0) {
			Lx_kit::env().initcalls.add(initcall, i);
			return;
		}
	}
	Genode::error("Initcall ", name, " unknown in initcall database!");
}


extern "C" void lx_emul_start_kernel(void * dtb)
{
	using namespace Lx_kit;

	/* register 'module_init' calls and friends */
	lx_emul_register_initcalls();

	new (env().heap) Task(lx_emul_init_task_function, dtb,
	                      lx_emul_init_task_struct, SWAPPER_PID, "swapper",
	                      env().scheduler, Task::TIME_HANDLER);
	new (env().heap) Task(lx_emul_irq_task_function, nullptr,
	                      lx_emul_irq_task_struct, KIRQ_PID, "kirqd",
	                      env().scheduler, Task::IRQ_HANDLER);

	env().scheduler.schedule();
}


extern "C" void lx_emul_execute_kernel_until(int (*condition)(void*), void * args)
{
	do {
		/*
		 * Assume we have to execute all scheduled tasks once before
		 * it makes sense to check the condition.
		 */
		Lx_kit::env().scheduler.execute();

		if (condition(args))
			break;

		Lx_kit::env().env.ep().wait_and_dispatch_one_io_signal();
	} while (true);
}
