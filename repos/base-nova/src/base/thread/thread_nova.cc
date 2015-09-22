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
	using namespace Genode;

	/* catch any exception at this point and try to print an error message */
	try {
		Thread_base::myself()->entry();
	} catch (...) {
		char thread_name[48];
		Thread_base::myself()->name(thread_name, sizeof(thread_name));

		try {
			PERR("Thread '%s' died because of an uncaught exception", thread_name);
		} catch (...) {
			/* die in a noisy way */
			nova_die();
		}

		throw;
	}

	Thread_base::myself()->_join_lock.unlock();

	/* sleep silently */
	Genode::sleep_forever();
}


/*****************
 ** Thread base **
 *****************/

void Thread_base::_init_platform_thread(size_t weight, Type type)
{
	using namespace Nova;

	/*
	 * Allocate capability selectors for the thread's execution context,
	 * running semaphore and exception handler portals.
	 */
	_tid.ec_sel = Native_thread::INVALID_INDEX;

	/* for main threads the member initialization differs */
	if (type == MAIN || type == REINITIALIZED_MAIN) {
		_thread_cap = env()->parent()->main_thread_cap();

		Genode::Native_capability pager_cap(Nova::PT_SEL_MAIN_PAGER);
		_pager_cap  = reinterpret_cap_cast<Pager_object>(pager_cap);

		_tid.exc_pt_sel = 0;
		_tid.ec_sel     = Nova::PT_SEL_MAIN_EC;

		request_native_ec_cap(_pager_cap, _tid.ec_sel);
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
	addr_t utcb = reinterpret_cast<addr_t>(&_context->utcb);
	revoke(Mem_crd(utcb >> 12, 0, rwx));

	_tid.exc_pt_sel = cap_map()->insert(NUM_INITIAL_PT_LOG2);
	if (_tid.exc_pt_sel == Native_thread::INVALID_INDEX)
		throw Cpu_session::Thread_creation_failed();

	/* if no cpu session is given, use it from the environment */
	if (!_cpu_session)
		_cpu_session = env()->cpu_session();

	/* create thread at core */
	char buf[48];
	name(buf, sizeof(buf));

	_thread_cap = _cpu_session->create_thread(weight, buf);
	if (!_thread_cap.valid())
		throw Cpu_session::Thread_creation_failed();

	/* assign thread to protection domain */
	if (env()->pd_session()->bind_thread(_thread_cap))
		throw Cpu_session::Thread_creation_failed();

}


void Thread_base::_deinit_platform_thread()
{
	using namespace Nova;

	if (_tid.ec_sel != Native_thread::INVALID_INDEX) {
		revoke(Obj_crd(_tid.ec_sel, 1));
		cap_map()->remove(_tid.ec_sel, 1, false);
	}

	/* de-announce thread */
	if (_thread_cap.valid())
		_cpu_session->kill_thread(_thread_cap);

	cap_map()->remove(_tid.exc_pt_sel, NUM_INITIAL_PT_LOG2);

	if (_pager_cap.valid())
		env()->rm_session()->remove_client(_pager_cap);
}


void Thread_base::start()
{
	if (_tid.ec_sel < Native_thread::INVALID_INDEX - 1)
		throw Cpu_session::Thread_creation_failed();

	/*
	 * Default: create global thread - ec.sel == INVALID_INDEX
	 *          create  local thread - ec.sel == INVALID_INDEX - 1
	 */ 
	bool global = _tid.ec_sel == Native_thread::INVALID_INDEX;

	using namespace Genode;

	/* create new pager object and assign it to the new thread */
	_pager_cap = env()->rm_session()->add_client(_thread_cap);
	if (!_pager_cap.valid())
		throw Cpu_session::Thread_creation_failed();

	if (_cpu_session->set_pager(_thread_cap, _pager_cap))
		throw Cpu_session::Thread_creation_failed();

	/* create EC at core */
	Thread_state state;
	state.sel_exc_base = _tid.exc_pt_sel;
	state.is_vcpu      = _tid.is_vcpu;

	/* local thread have no start instruction pointer - set via portal entry */
	addr_t thread_ip = global ? reinterpret_cast<addr_t>(_thread_start) : 0;

	try { _cpu_session->state(_thread_cap, state); }
	catch (...) { throw Cpu_session::Thread_creation_failed(); }

	if (_cpu_session->start(_thread_cap, thread_ip, _context->stack_top()))
		throw Cpu_session::Thread_creation_failed();

	/* request native EC thread cap */ 
	_tid.ec_sel = cap_map()->insert(1);
	if (_tid.ec_sel == Native_thread::INVALID_INDEX)
		throw Cpu_session::Thread_creation_failed();

	/* requested pager cap used by request_native_ec_cap in Signal_source_client */
	enum { MAP_PAGER_CAP = 1 };
	request_native_ec_cap(_pager_cap, _tid.ec_sel, MAP_PAGER_CAP);

	using namespace Nova;

	/* request exception portals for normal threads */
	if (!_tid.is_vcpu) {
		request_event_portal(_pager_cap, _tid.exc_pt_sel, 0, NUM_INITIAL_PT_LOG2);

		/* default: we don't accept any mappings or translations */
		Utcb * utcb_obj = reinterpret_cast<Utcb *>(utcb());
		utcb_obj->crd_rcv = Obj_crd();
		utcb_obj->crd_xlt = Obj_crd();
	}

	if (global)
		/* request creation of SC to let thread run*/
		_cpu_session->resume(_thread_cap);
}


void Thread_base::cancel_blocking()
{
	using namespace Nova;

	if (sm_ctrl(_tid.exc_pt_sel + SM_SEL_EC, SEMAPHORE_UP))
		nova_die();
}
