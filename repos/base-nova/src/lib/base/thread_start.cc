/*
 * \brief  NOVA-specific implementation of the Thread API
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \date   2010-01-19
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/log.h>
#include <base/sleep.h>
#include <base/env.h>
#include <base/rpc_client.h>
#include <session/session.h>
#include <cpu_thread/client.h>
#include <nova_native_cpu/client.h>

/* base-internal includes */
#include <base/internal/stack.h>
#include <base/internal/globals.h>

/* NOVA includes */
#include <nova/syscalls.h>
#include <nova/util.h>
#include <nova/cap_map.h>
#include <nova/capability_space.h>

using namespace Genode;


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

	Thread::myself()->_join.wakeup();

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
	if (type == MAIN) {
		_thread_cap = main_thread_cap();

		native_thread().exc_pt_sel = 0;
		native_thread().ec_sel     = Nova::EC_SEL_THREAD;

		request_native_ec_cap(PT_SEL_PAGE_FAULT, native_thread().ec_sel);
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

	native_thread().exc_pt_sel = cap_map().insert(NUM_INITIAL_PT_LOG2);
	if (native_thread().exc_pt_sel == Native_thread::INVALID_INDEX)
		throw Cpu_session::Thread_creation_failed();


	_init_cpu_session_and_trace_control();

	/* create thread at core */
	_thread_cap = _cpu_session->create_thread(pd_session_cap(), name(),
	                                          _affinity, Weight(weight));
	if (!_thread_cap.valid())
		throw Cpu_session::Thread_creation_failed();
}


void Thread::_deinit_platform_thread()
{
	using namespace Nova;

	if (native_thread().ec_sel != Native_thread::INVALID_INDEX) {
		revoke(Obj_crd(native_thread().ec_sel, 0));
	}

	/* de-announce thread */
	if (_thread_cap.valid())
		_cpu_session->kill_thread(_thread_cap);

	cap_map().remove(native_thread().exc_pt_sel, NUM_INITIAL_PT_LOG2);
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

	/* create EC at core */

	try {
		Cpu_session::Native_cpu::Thread_type thread_type;

		if (global)
			thread_type = Cpu_session::Native_cpu::Thread_type::GLOBAL;
		else
			thread_type = Cpu_session::Native_cpu::Thread_type::LOCAL;

		Cpu_session::Native_cpu::Exception_base exception_base { native_thread().exc_pt_sel };

		Nova_native_cpu_client native_cpu(_cpu_session->native_cpu());
		native_cpu.thread_type(_thread_cap, thread_type, exception_base);
	} catch (...) { throw Cpu_session::Thread_creation_failed(); }

	/* local thread have no start instruction pointer - set via portal entry */
	addr_t thread_ip = global ? reinterpret_cast<addr_t>(_thread_start) : native_thread().initial_ip;

	Cpu_thread_client cpu_thread(_thread_cap);
	cpu_thread.start(thread_ip, _stack->top());

	/* request native EC thread cap */ 
	native_thread().ec_sel = native_thread().exc_pt_sel + Nova::EC_SEL_THREAD;

	/*
	 * Requested ec cap that is used for recall and
	 * creation of portals (Native_pd::alloc_rpc_cap).
	 */
	request_native_ec_cap(native_thread().exc_pt_sel + Nova::PT_SEL_PAGE_FAULT,
	                      native_thread().ec_sel);

	using namespace Nova;

	/* default: we don't accept any mappings or translations */
	Utcb * utcb_obj = reinterpret_cast<Utcb *>(utcb());
	utcb_obj->crd_rcv = Obj_crd();
	utcb_obj->crd_xlt = Obj_crd();

	if (global)
		/* request creation of SC to let thread run*/
		cpu_thread.resume();
}


void Genode::init_thread_start(Capability<Pd_session> pd_cap)
{
	pd_session_cap(pd_cap);
}


void Genode::init_thread_bootstrap(Thread_capability main_cap)
{
	main_thread_cap(main_cap);
}
