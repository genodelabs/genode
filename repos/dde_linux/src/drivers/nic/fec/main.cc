/*
 * \brief  Freescale ethernet driver
 * \author Stefan Kalkowski
 * \date   2017-10-19
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/component.h>
#include <base/heap.h>

#include <component.h>

/* Linux emulation environment includes */
#include <lx_emul.h>
#include <lx_kit/env.h>
#include <lx_kit/malloc.h>
#include <lx_kit/scheduler.h>
#include <lx_kit/timer.h>
#include <lx_kit/irq.h>
#include <lx_kit/backend_alloc.h>
#include <lx_kit/work.h>

/* Linux module functions */
extern "C" int module_fec_driver_init();
extern "C" int module_phy_module_init();
extern "C" int subsys_phy_init();
extern "C" void skb_init();

static void run_linux(void * m);

struct workqueue_struct *system_wq;
struct workqueue_struct *system_power_efficient_wq;
unsigned long jiffies;

struct Main
{
	Genode::Env        &env;
	Genode::Entrypoint &ep     { env.ep() };
	Genode::Heap        heap   { env.ram(), env.rm() };
	Root                root   { env, heap };

	/* Linux task that handles the initialization */
	Genode::Constructible<Lx::Task> linux;

	Main(Genode::Env &env) : env(env)
	{
		Genode::log("--- freescale ethernet driver ---");

		Lx_kit::construct_env(env);

		LX_MUTEX_INIT(mdio_board_lock);
		LX_MUTEX_INIT(phy_fixup_lock);

		/* init singleton Lx::Scheduler */
		Lx::scheduler(&env);

		Lx::malloc_init(env, heap);

		/* init singleton Lx::Timer */
		Lx::timer(&env, &ep, &heap, &jiffies);

		/* init singleton Lx::Irq */
		Lx::Irq::irq(&ep, &heap);

		/* init singleton Lx::Work */
		Lx::Work::work_queue(&heap);

		linux.construct(run_linux, reinterpret_cast<void*>(this),
		                "linux", Lx::Task::PRIORITY_0, Lx::scheduler());

		/* give all task a first kick before returning */
		Lx::scheduler().schedule();
	}

	void announce() { env.parent().announce(ep.manage(root)); }

	Lx::Task &linux_task() { return *linux; }
};


static void run_linux(void * m)
{
	Main & main = *reinterpret_cast<Main*>(m);

	system_wq                 = (workqueue_struct *)kzalloc(sizeof(workqueue_struct), 0);
	system_power_efficient_wq = (workqueue_struct *)kzalloc(sizeof(workqueue_struct), 0);

	skb_init();
	subsys_phy_init();
	module_phy_module_init();
	module_fec_driver_init();

	main.announce();
	while (1) Lx::scheduler().current()->block_and_schedule();
}


void Component::construct(Genode::Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	static Main m(env);
}
