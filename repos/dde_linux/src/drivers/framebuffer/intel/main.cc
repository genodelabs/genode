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

/* Linux emulation environment includes */
#include <lx_emul.h>
#include <lx_emul/impl/internal/scheduler.h>
#include <lx_emul/impl/internal/timer.h>
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


extern "C" int postcore_i2c_init(); /* i2c-core.c */
extern "C" int module_i915_init();  /* i915_drv.c */


static void run_linux(void *)
{
	PDBG("postcore_i915_init");
	postcore_i2c_init();

	PDBG("module_i915_init");
	module_i915_init();

	while (1) {
		Lx::scheduler().current()->block_and_schedule();
	}
}


unsigned long jiffies;


struct Server::Main
{
	Entrypoint &ep;

	/* init singleton Lx::Timer */
	Lx::Timer &timer = Lx::timer(&ep, &jiffies);

	/* Linux task that handles the initialization */
	Lx::Task linux { run_linux, nullptr, "linux",
	                 Lx::Task::PRIORITY_0, Lx::scheduler() };

	Main(Entrypoint &ep) : ep(ep)
	{
		Genode::printf("--- intel framebuffer driver ---\n");

		/* give all task a first kick before returning */
		Lx::scheduler().schedule();

		PDBG("returning from main");
	}
};


namespace Server {

	char const *name() { return "intel_fb_ep"; }

	size_t stack_size() { return 8*1024*sizeof(long); }

	void construct(Entrypoint &ep)
	{
		static Main main(ep);
	}
}
