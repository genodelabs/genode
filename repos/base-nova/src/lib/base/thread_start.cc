/*
 * \brief  NOVA-specific implementation of the Thread API
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \date   2010-01-19
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/log.h>
#include <base/sleep.h>
#include <base/env.h>
#include <base/rpc_client.h>
#include <session/session.h>
#include <nova_native_cpu/client.h>
#include <cpu_thread/client.h>

/* base-internal includes */
#include <base/internal/stack.h>

/* NOVA includes */
#include <nova/syscalls.h>
#include <nova/util.h>
#include <nova/cap_map.h>
#include <nova/capability_space.h>

using namespace Genode;


/**
 * Entry point entered by new threads
 */
void Thread::_thread_start()
{
	using namespace Genode;

	/* catch any exception at this point and try to print an error message */
	try {
		Thread::myself()->entry();
	} catch (...) {
		try {
			error("Thread '", Thread::myself()->name(), "' "
			      "died because of an uncaught exception");
		} catch (...) {
			/* die in a noisy way */
			nova_die();
		}

		throw;
	}

	Thread::myself()->_join_lock.unlock();

	/* sleep silently */
	Genode::sleep_forever();
}


/*****************
 ** Thread base **
 *****************/

void Thread::_init_platform_thread(size_t weight, Type type)
{
	using namespace Nova;

	/*
	 * Allocate capability selectors for the thread's execution context,
	 * running semaphore and exception handler portals.
	 */
	native_thread().ec_sel = Native_thread::INVALID_INDEX;

	/* for main threads the member initialization differs */
	if (type == MAIN || type == REINITIALIZED_MAIN) {
		_thread_cap = env()->parent()->main_thread_cap();

		Genode::Native_capability pager_cap =
			Capability_space::import(Nova::PT_SEL_MAIN_PAGER);

		native_thread().exc_pt_sel = 0;
		native_thread().ec_sel     = Nova::PT_SEL_MAIN_EC;

		request_native_ec_cap(pager_cap, native_thread().ec_sel);
		return;
	}

	/*
	 * Revoke possible left-over UTCB of a previously destroyed thread
	 * which used this context location.
	 *
	 * This cannot be done in '_deinit_platform_thread()', because a
	 * self-destructing thread needs its UTCB to call
	 * 'Cpu_session::kill_thread()' and is not able to revoke the UTCB
	 * afterwards.
	 */
	Rights rwx(true, true, true);
	addr_t utcb = reinterpret_cast<addr_t>(&_stack->utcb());
	revoke(Mem_crd(utcb >> 12, 0, rwx));

	native_thread().exc_pt_sel = cap_map()->insert(NUM_INITIAL_PT_LOG2);
	if (native_thread().exc_pt_sel == Native_thread::INVALID_INDEX)
		throw Cpu_session::Thread_creation_failed();

	/* if no cpu session is given, use it from the environment */
	if (!_cpu_session)
		_cpu_session = env()->cpu_session();

	/* create thread at core */
	_thread_cap = _cpu_session->create_thread(env()->pd_session_cap(), name(),
	                                          _affinity, Weight(weight));
	if (!_thread_cap.valid())
		throw Cpu_session::Thread_creation_failed();
}


void Thread::_deinit_platform_thread()
{
	using namespace Nova;

	if (native_thread().ec_sel != Native_thread::INVALID_INDEX) {
		revoke(Obj_crd(native_thread().ec_sel, 1));
		cap_map()->remove(native_thread().ec_sel, 1, false);
	}

	/* de-announce thread */
	if (_thread_cap.valid())
		_cpu_session->kill_thread(_thread_cap);

	cap_map()->remove(native_thread().exc_pt_sel, NUM_INITIAL_PT_LOG2);
}


void Thread::start()
{
	if (native_thread().ec_sel < Native_thread::INVALID_INDEX - 1)
		throw Cpu_session::Thread_creation_failed();

	/*
	 * Default: create global thread - ec.sel == INVALID_INDEX
	 *          create  local thread - ec.sel == INVALID_INDEX - 1
	 */ 
	bool global = native_thread().ec_sel == Native_thread::INVALID_INDEX;

	using namespace Genode;

	/* obtain interface to NOVA-specific CPU session operations */
	Nova_native_cpu_client native_cpu(_cpu_session->native_cpu());

	/* create new pager object and assign it to the new thread */
	Native_capability pager_cap = native_cpu.pager_cap(_thread_cap);
	if (!pager_cap.valid())
		throw Cpu_session::Thread_creation_failed();

	/* create EC at core */
	Thread_state state;
	state.sel_exc_base  = native_thread().exc_pt_sel;
	state.vcpu          = native_thread().vcpu;
	state.global_thread = global;

	/* local thread have no start instruction pointer - set via portal entry */
	addr_t thread_ip = global ? reinterpret_cast<addr_t>(_thread_start) : native_thread().initial_ip;

	Cpu_thread_client cpu_thread(_thread_cap);
	try { cpu_thread.state(state); }
	catch (...) { throw Cpu_session::Thread_creation_failed(); }

	cpu_thread.start(thread_ip, _stack->top());

	/* request native EC thread cap */ 
	native_thread().ec_sel = cap_map()->insert(1);
	if (native_thread().ec_sel == Native_thread::INVALID_INDEX)
		throw Cpu_session::Thread_creation_failed();

	/* requested pager cap used by request_native_ec_cap in Signal_source_client */
	enum { MAP_PAGER_CAP = 1 };
	request_native_ec_cap(pager_cap, native_thread().ec_sel, MAP_PAGER_CAP);

	using namespace Nova;

	/* request exception portals for normal threads */
	if (!native_thread().vcpu) {
		request_event_portal(pager_cap, native_thread().exc_pt_sel, 0, NUM_INITIAL_PT_LOG2);

		/* default: we don't accept any mappings or translations */
		Utcb * utcb_obj = reinterpret_cast<Utcb *>(utcb());
		utcb_obj->crd_rcv = Obj_crd();
		utcb_obj->crd_xlt = Obj_crd();
	}

	if (global)
		/* request creation of SC to let thread run*/
		cpu_thread.resume();
}


void Thread::cancel_blocking()
{
	using namespace Nova;

	if (sm_ctrl(native_thread().exc_pt_sel + SM_SEL_EC, SEMAPHORE_UP))
		nova_die();
}
