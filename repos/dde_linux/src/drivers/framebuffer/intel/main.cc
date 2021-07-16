/*
 * \brief  Intel framebuffer driver
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2015-08-19
 */

/*
 * Copyright (C) 2015-2020 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>

/* local includes */
#include <driver.h>

/* Linux emulation environment includes */
#include <lx_emul.h>
#include <legacy/lx_kit/env.h>
#include <legacy/lx_kit/malloc.h>
#include <legacy/lx_kit/scheduler.h>
#include <legacy/lx_kit/timer.h>
#include <legacy/lx_kit/irq.h>
#include <legacy/lx_kit/pci_dev_registry.h>
#include <legacy/lx_kit/backend_alloc.h>
#include <legacy/lx_kit/work.h>

/* Linux module functions */
extern "C" void postcore_i2c_init(void);  /* i2c-core-base.c */
extern "C" int module_i915_init();        /* i915_drv.c */
extern "C" void radix_tree_init();        /* called by start_kernel(void) normally */
extern "C" void drm_connector_ida_init(); /* called by drm_core_init(void) normally */

unsigned long jiffies;

namespace Framebuffer { struct Main; }


struct Framebuffer::Main
{
	void _run_linux();

	/**
	 * Entry for executing code in the Linux kernel context
	 */
	static void _run_linux_entry(void *m)
	{
		reinterpret_cast<Main*>(m)->_run_linux();
	}

	Env                   &_env;
	Entrypoint            &_ep     { _env.ep() };
	Attached_rom_dataspace _config { _env, "config" };
	Heap                   _heap   { _env.ram(), _env.rm() };
	Driver                 _driver { _env, _config };

	/* Linux task that handles the initialization */
	Constructible<Lx::Task> _linux;

	Signal_handler<Main> _policy_change_handler {
		_env.ep(), *this, &Main::_handle_policy_change };

	bool _policy_change_pending = false;

	void _handle_policy_change()
	{
		_policy_change_pending = true;
		_linux->unblock();
		Lx::scheduler().schedule();
	}

	Main(Env &env) : _env(env)
	{
		log("--- intel framebuffer driver ---");

		Lx_kit::construct_env(_env);

		LX_MUTEX_INIT(bridge_lock);
		LX_MUTEX_INIT(core_lock);

		/* init singleton Lx::Scheduler */
		Lx::scheduler(&_env);

		Lx::pci_init(_env, _env.ram(), _heap);
		Lx::malloc_init(_env, _heap);

		/* init singleton Lx::Timer */
		Lx::timer(&_env, &_ep, &_heap, &jiffies);

		/* init singleton Lx::Irq */
		Lx::Irq::irq(&_ep, &_heap);

		/* init singleton Lx::Work */
		Lx::Work::work_queue(&_heap);

		_linux.construct(_run_linux_entry, reinterpret_cast<void*>(this),
		                 "linux", Lx::Task::PRIORITY_0, Lx::scheduler());

		/* give all task a first kick before returning */
		Lx::scheduler().schedule();
	}
};


void Framebuffer::Main::_run_linux()
{
	system_wq  = alloc_workqueue("system_wq", 0, 0);

	radix_tree_init();
	drm_connector_ida_init();
	postcore_i2c_init();
	module_i915_init();

	_driver.finish_initialization();
	_driver.config_sigh(_policy_change_handler);

	_config.sigh(_policy_change_handler);

	while (1) {
		Lx::scheduler().current()->block_and_schedule();
		while (_policy_change_pending) {
			_policy_change_pending = false;
			_driver.config_changed();
		}
	}
}


void Component::construct(Genode::Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	static Framebuffer::Main main(env);
}
