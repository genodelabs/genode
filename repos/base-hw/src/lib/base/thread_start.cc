/**
 * \brief  Implementations for the start of a thread
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2012-02-12
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/log.h>
#include <base/sleep.h>
#include <base/env.h>
#include <cpu_thread/client.h>

/* base-internal includes */
#include <base/internal/stack_allocator.h>
#include <base/internal/native_utcb.h>
#include <base/internal/globals.h>

using namespace Genode;

namespace Hw {
	extern Ram_dataspace_capability _main_thread_utcb_ds;
	extern Untyped_capability       _main_thread_cap;
}

/************
 ** Thread **
 ************/

void Thread::_init_platform_thread(size_t weight, Type type)
{
	if (!_cpu_session) { _cpu_session = env()->cpu_session(); }
	if (type == NORMAL) {

		/* create server object */
		addr_t const utcb = (addr_t)&_stack->utcb();
		_thread_cap = _cpu_session->create_thread(env()->pd_session_cap(),
		                                          name(), _affinity,
		                                          Weight(weight), utcb);
		return;
	}
	/* if we got reinitialized we have to get rid of the old UTCB */
	size_t const utcb_size  = sizeof(Native_utcb);
	addr_t const stack_area = stack_area_virtual_base();
	addr_t const utcb_new   = (addr_t)&_stack->utcb() - stack_area;
	Region_map * const rm   = env_stack_area_region_map;

	if (type == REINITIALIZED_MAIN) { rm->detach(utcb_new); }

	/* remap initial main-thread UTCB according to stack-area spec */
	try { rm->attach_at(Hw::_main_thread_utcb_ds, utcb_new, utcb_size); }
	catch(...) {
		error("failed to re-map UTCB");
		while (1) ;
	}
	/* adjust initial object state in case of a main thread */
	native_thread().cap = Hw::_main_thread_cap;
	_thread_cap = env()->parent()->main_thread_cap();
}


void Thread::_deinit_platform_thread()
{
	if (!_cpu_session)
		_cpu_session = env()->cpu_session();

	_cpu_session->kill_thread(_thread_cap);

	/* detach userland stack */
	size_t const size = sizeof(_stack->utcb());
	addr_t utcb = Stack_allocator::addr_to_base(_stack) +
	              stack_virtual_size() - size - stack_area_virtual_base();
	env_stack_area_region_map->detach(utcb);
}


void Thread::start()
{
	/* attach userland stack */
	try {
		Dataspace_capability ds = Cpu_thread_client(_thread_cap).utcb();
		size_t const size = sizeof(_stack->utcb());
		addr_t dst = Stack_allocator::addr_to_base(_stack) +
		             stack_virtual_size() - size - stack_area_virtual_base();
		env_stack_area_region_map->attach_at(ds, dst, size);
	} catch (...) {
		error("failed to attach userland stack");
		sleep_forever();
	}
	/* start thread with its initial IP and aligned SP */
	Cpu_thread_client(_thread_cap).start((addr_t)_thread_start, _stack->top());
}


void Thread::cancel_blocking()
{
	Cpu_thread_client(_thread_cap).cancel_blocking();
}
