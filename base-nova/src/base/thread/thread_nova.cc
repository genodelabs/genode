/*
 * \brief  NOVA-specific implementation of the Thread API
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \date   2010-01-19
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/cap_sel_alloc.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <base/env.h>

#include <base/rpc_client.h>
#include <session/session.h>

/* NOVA includes */
#include <nova/syscalls.h>
#include <nova/util.h>
#include <nova_cpu_session/connection.h>

using namespace Genode;


/**
 * Entry point entered by new threads
 */
void Thread_base::_thread_start()
{
	Genode::Thread_base::myself()->entry();
	Genode::sleep_forever();
}


/*****************
 ** Thread base **
 *****************/

void Thread_base::_init_platform_thread()
{
	using namespace Nova;

	/*
	 * Allocate capability selectors for the thread's execution context,
	 * running semaphore and exception handler portals.
	 */
	_tid.ec_sel     = ~0UL;
	_tid.exc_pt_sel = cap_selector_allocator()->alloc(NUM_INITIAL_PT_LOG2);

	/* create thread at core */
	char buf[48];
	name(buf, sizeof(buf));

	_thread_cap = env()->cpu_session()->create_thread(buf);
	if (!_thread_cap.valid())
		throw Cpu_session::Thread_creation_failed();

	/* assign thread to protection domain */
	if (env()->pd_session()->bind_thread(_thread_cap))
		throw Cpu_session::Thread_creation_failed();

}


void Thread_base::_deinit_platform_thread()
{
	using namespace Nova;

	if (_tid.ec_sel != ~0UL) {
		revoke(Obj_crd(_tid.ec_sel, 0));
		cap_selector_allocator()->free(_tid.ec_sel, 0);
	}

	revoke(Obj_crd(_tid.exc_pt_sel, NUM_INITIAL_PT_LOG2));
	cap_selector_allocator()->free(_tid.exc_pt_sel, NUM_INITIAL_PT_LOG2);

	/* revoke utcb */
	Rights rwx(true, true, true);
	addr_t utcb = reinterpret_cast<addr_t>(&_context->utcb);
	revoke(Mem_crd(utcb >> 12, 0, rwx));

	/* de-announce thread */
	env()->cpu_session()->kill_thread(_thread_cap);

	revoke(_thread_cap.local_name(), 0);
	cap_selector_allocator()->free(_thread_cap.local_name(), 0);

}


void Thread_base::start()
{
	if (_tid.ec_sel != ~0UL)
		throw Cpu_session::Thread_creation_failed();

	using namespace Genode;

	/* create new pager object and assign it to the new thread */
	Pager_capability pager_cap =
		env()->rm_session()->add_client(_thread_cap);
	if (!pager_cap.valid())
		throw Cpu_session::Thread_creation_failed();

	if (env()->cpu_session()->set_pager(_thread_cap, pager_cap))
		throw Cpu_session::Thread_creation_failed();

	/* create EC at core */
	addr_t thread_sp = reinterpret_cast<addr_t>(&_context->stack[-4]);

	Thread_state state(true);
	state.sel_exc_base = _tid.exc_pt_sel;
	state.is_vcpu      = _tid.is_vcpu;

	if (env()->cpu_session()->state(_thread_cap, &state) ||
	    env()->cpu_session()->start(_thread_cap, (addr_t)_thread_start,
	                                thread_sp))
		throw Cpu_session::Thread_creation_failed();

	/* request native EC thread cap */ 
	Genode::Nova_cpu_connection cpu;
	Native_capability ec_cap = cpu.native_cap(_thread_cap);
	if (!ec_cap.valid())
		throw Cpu_session::Thread_creation_failed();
	_tid.ec_sel = ec_cap.local_name();

	using namespace Nova;

	/* request exception portals for normal threads */
	if (!_tid.is_vcpu) {
		for (unsigned i = 0; i < PT_SEL_PARENT; i++)
			request_event_portal(pager_cap, _tid.exc_pt_sel, i);

		request_event_portal(pager_cap, _tid.exc_pt_sel, PT_SEL_STARTUP);
		request_event_portal(pager_cap, _tid.exc_pt_sel, SM_SEL_EC);
		request_event_portal(pager_cap, _tid.exc_pt_sel, PT_SEL_RECALL);
	}

	/* request creation of SC to let thread run*/
	env()->cpu_session()->resume(_thread_cap);
}


void Thread_base::cancel_blocking()
{
	using namespace Nova;

	if (sm_ctrl(_tid.exc_pt_sel + SM_SEL_EC, SEMAPHORE_UP))
		nova_die();
}
