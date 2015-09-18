/*
 * \brief  Intel framebuffer driver
 * \author Norman Feske
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

#include <component.h>

/* Linux emulation environment includes */
#include <lx_emul.h>
#include <lx_emul/impl/internal/scheduler.h>
#include <lx_emul/impl/internal/timer.h>
#include <lx_emul/impl/internal/irq.h>
#include <lx_emul/impl/internal/pci_dev_registry.h>
#include <lx_emul/impl/internal/pci_backend_alloc.h>


namespace Server { struct Main; }


Lx::Scheduler & Lx::scheduler()
{
	static Lx::Scheduler inst;
	return inst;
}


Lx::Timer & Lx::timer(Server::Entrypoint *ep, unsigned long *jiffies)
{
	return _timer_impl(ep, jiffies);
}


Lx::Irq & Lx::Irq::irq(Server::Entrypoint *ep)
{
	static Lx::Irq irq(*ep);
	return irq;
}


Platform::Connection *Lx::pci()
{
	static Platform::Connection _pci;
	return &_pci;
}


Lx::Pci_dev_registry *Lx::pci_dev_registry()
{
	static Lx::Pci_dev_registry _pci_dev_registry;
	return &_pci_dev_registry;
}


namespace Lx {
	Genode::Object_pool<Memory_object_base> memory_pool;
};


Framebuffer::Root * Framebuffer::root = nullptr;


extern "C" int postcore_i2c_init(); /* i2c-core.c */
extern "C" int module_i915_init();  /* i915_drv.c */
extern "C" void update_framebuffer_config();


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
	Lx::Irq &irq = Lx::Irq::irq(&ep);

	/* Linux task that handles the initialization */
	Lx::Task linux { run_linux, reinterpret_cast<void*>(this), "linux",
	                 Lx::Task::PRIORITY_0, Lx::scheduler() };

	Main(Entrypoint &ep) : ep(ep)
	{
		Genode::printf("--- intel framebuffer driver ---\n");

		Framebuffer::root = &root_component;

		/* give all task a first kick before returning */
		Lx::scheduler().schedule();
	}
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

	Genode::env()->parent()->announce(main->ep.manage(*Framebuffer::root));

	static Policy_agent pa(*main);
	Genode::config()->sigh(pa.sd);

	while (1) {
		Lx::scheduler().current()->block_and_schedule();
		update_framebuffer_config();
	}
}

namespace Server {

	char const *name() { return "intel_fb_ep"; }

	size_t stack_size() { return 8*1024*sizeof(long); }

	void construct(Entrypoint &ep)
	{
		static Main main(ep);
	}
}
