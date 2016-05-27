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
#include <base/printf.h>
#include <os/server.h>
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


namespace Server { struct Main; }

static void run_linux(void * m);

unsigned long jiffies;


struct Server::Main
{
	Entrypoint &ep;

	bool _buffered_from_config()
	{
		try {
			config()->reload();
			return Genode::config()->xml_node().attribute_value("buffered",
			                                                    false);
		} catch (...) { return false; }
	}

	Framebuffer::Root root_component { &ep.rpc_ep(), Genode::env()->heap(),
	                                   _buffered_from_config() };

	/* init singleton Lx::Timer */
	Lx::Timer &timer = Lx::timer(&ep, &jiffies);

	/* init singleton Lx::Irq */
	Lx::Irq &irq = Lx::Irq::irq(&ep, Genode::env()->heap());

	/* init singleton Lx::Work */
	Lx::Work &work = Lx::Work::work_queue(Genode::env()->heap());

	/* Linux task that handles the initialization */
	Lx::Task linux { run_linux, reinterpret_cast<void*>(this), "linux",
	                 Lx::Task::PRIORITY_0, Lx::scheduler() };

	Main(Entrypoint &ep) : ep(ep)
	{
		Genode::printf("--- intel framebuffer driver ---\n");

		/* give all task a first kick before returning */
		Lx::scheduler().schedule();
	}

	void announce() {
		Genode::env()->parent()->announce(ep.manage(root_component)); }
};


struct Policy_agent
{
	Server::Main &main;
	Genode::Signal_rpc_member<Policy_agent> sd;

	void handle(unsigned)
	{
		main.linux.unblock();
		Lx::scheduler().schedule();
	}

	Policy_agent(Server::Main &m)
	: main(m), sd(main.ep, *this, &Policy_agent::handle) {}
};


static void run_linux(void * m)
{
	Server::Main * main = reinterpret_cast<Server::Main*>(m);

	postcore_i2c_init();
	module_i915_init();
	main->root_component.session.driver().finish_initialization();
	main->announce();

	static Policy_agent pa(*main);
	Genode::config()->sigh(pa.sd);

	while (1) {
		Lx::scheduler().current()->block_and_schedule();
		main->root_component.session.config_changed();
	}
}


namespace Server {

	char const *name() { return "intel_fb_ep"; }

	size_t stack_size() { return 8*1024*sizeof(long); }

	void construct(Entrypoint &ep) { static Main m(ep); }
}
