/**
 * \brief  Implementations for the start of a thread
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2012-02-12
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
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
#include <base/internal/native_env.h>
#include <base/internal/globals.h>

using namespace Genode;

namespace Hw {
	extern Ram_dataspace_capability _main_thread_utcb_ds;
	extern Untyped_capability       _main_thread_cap;
}


static Capability<Pd_session> pd_session_cap(Capability<Pd_session> pd_cap = { })
{
	static Capability<Pd_session> cap = pd_cap; /* defined once by 'init_thread_start' */
	return cap;
}


static Thread_capability main_thread_cap(Thread_capability main_cap = { })
{
	static Thread_capability cap = main_cap;
	return cap;
}


/************
 ** Thread **
 ************/

void Thread::_init_platform_thread(size_t weight, Type type)
{
	_init_cpu_session_and_trace_control();

	if (type == NORMAL) {

		/* create server object */
		addr_t const utcb = (addr_t)&_stack->utcb();

		_thread_cap = _cpu_session->create_thread(pd_session_cap(), name(), _affinity,
		                                          Weight(weight), utcb);
		return;
	}
	/* if we got reinitialized we have to get rid of the old UTCB */
	size_t const utcb_size  = sizeof(Native_utcb);
	addr_t const stack_area = stack_area_virtual_base();
	addr_t const utcb_new   = (addr_t)&_stack->utcb() - stack_area;

	/* remap initial main-thread UTCB according to stack-area spec */
	if (env_stack_area_region_map->attach(Hw::_main_thread_utcb_ds, {
		.size       = utcb_size,
		.offset     = { },
		.use_at     = true,
		.at         = utcb_new,
		.executable = { },
		.writeable  = true
	}).failed())
		error("failed to attach UTCB to local address space");

	/* adjust initial object state in case of a main thread */
	native_thread().cap = Hw::_main_thread_cap;
	_thread_cap = main_thread_cap();
}


void Thread::_deinit_platform_thread()
{
	if (!_cpu_session) {
		error("Thread::_cpu_session unexpectedly not defined");
		return;
	}

	_thread_cap.with_result(
		[&] (Thread_capability cap) { _cpu_session->kill_thread(cap); },
		[&] (Cpu_session::Create_thread_error) { });

	/* detach userland stack */
	size_t const size = sizeof(_stack->utcb());
	addr_t utcb = Stack_allocator::addr_to_base(_stack) +
	              stack_virtual_size() - size - stack_area_virtual_base();
	env_stack_area_region_map->detach(utcb);
}


Thread::Start_result Thread::start()
{
	while (avail_capability_slab() < 5)
		upgrade_capability_slab();

	return _thread_cap.convert<Start_result>(
		[&] (Thread_capability cap) {
			Cpu_thread_client cpu_thread(cap);

			/* attach UTCB at top of stack */
			size_t const size = sizeof(_stack->utcb());
			return env_stack_area_region_map->attach(cpu_thread.utcb(), {
				.size       = size,
				.offset     = { },
				.use_at     = true,
				.at         = Stack_allocator::addr_to_base(_stack)
				            + stack_virtual_size() - size - stack_area_virtual_base(),
				.executable = { },
				.writeable  = true
			}).convert<Start_result>(
				[&] (Region_map::Range) {
					/* start execution with initial IP and aligned SP */
					cpu_thread.start((addr_t)_thread_start, _stack->top());
					return Start_result::OK;
				},
				[&] (Region_map::Attach_error) {
					error("failed to attach userland stack");
					return Start_result::DENIED;
				}
			);
		},
		[&] (Cpu_session::Create_thread_error) { return Start_result::DENIED; }
	);
}


void Genode::init_thread_start(Capability<Pd_session> pd_cap)
{
	pd_session_cap(pd_cap);
}


void Genode::init_thread_bootstrap(Cpu_session &, Thread_capability main_cap)
{
	main_thread_cap(main_cap);
}
