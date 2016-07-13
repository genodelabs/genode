/*
 * \brief  Intel framebuffer driver
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2015-08-19
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <os/config.h>

/* Server related local includes */
#include <component.h>

/* Linux emulation environment includes */
#include <lx_emul.h>
#include <lx_kit/scheduler.h>
#include <lx_kit/timer.h>
#include <lx_kit/irq.h>
#include <lx_kit/pci_dev_registry.h>
#include <lx_kit/backend_alloc.h>
#include <lx_kit/work.h>

/* Linux module functions */
extern "C" int postcore_i2c_init(); /* i2c-core.c */
extern "C" int module_i915_init();  /* i915_drv.c */

static void run_linux(void * m);

unsigned long jiffies;


struct Main
{
	Genode::Env                   &env;
	Genode::Entrypoint            &ep     { env.ep() };
	Genode::Attached_rom_dataspace config { env, "config" };
	Genode::Heap                   heap   { env.ram(), env.rm() };
	Framebuffer::Root              root   { env, heap, config };

	/* init singleton Lx::Timer */
	Lx::Timer &timer = Lx::timer(&ep, &jiffies);

	/* init singleton Lx::Irq */
	Lx::Irq &irq = Lx::Irq::irq(&ep, &heap);

	/* init singleton Lx::Work */
	Lx::Work &work = Lx::Work::work_queue(&heap);

	/* Linux task that handles the initialization */
	Lx::Task linux { run_linux, reinterpret_cast<void*>(this), "linux",
	                 Lx::Task::PRIORITY_0, Lx::scheduler() };

	Main(Genode::Env &env) : env(env)
	{
		Genode::log("--- intel framebuffer driver ---");

		/* give all task a first kick before returning */
		Lx::scheduler().schedule();
	}

	void announce() { env.parent().announce(ep.manage(root)); }
};


struct Policy_agent
{
	Main &main;
	Genode::Signal_rpc_member<Policy_agent> sd;

	void handle(unsigned)
	{
		main.linux.unblock();
		Lx::scheduler().schedule();
	}

	Policy_agent(Main &m)
	: main(m), sd(main.ep, *this, &Policy_agent::handle) {}
};


static void run_linux(void * m)
{
	Main * main = reinterpret_cast<Main*>(m);

	postcore_i2c_init();
	module_i915_init();
	main->root.session.driver().finish_initialization();
	main->announce();

	static Policy_agent pa(*main);
	main->config.sigh(pa.sd);

	while (1) {
		Lx::scheduler().current()->block_and_schedule();
		main->root.session.config_changed();
	}
}


Genode::size_t Component::stack_size() {
	return 8*1024*sizeof(long); }


void Component::construct(Genode::Env &env) {
	static Main m(env); }
