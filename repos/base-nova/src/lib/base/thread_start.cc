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
#include <base/sleep.h>
#include <session/session.h>
#include <cpu_thread/client.h>
#include <nova_native_cpu/client.h>

/* base-internal includes */
#include <base/internal/stack.h>
#include <base/internal/runtime.h>

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
	/* catch any exception at this point and try to print an error message */
	auto on_exception = [&] {
		raw("Thread '", Thread::myself()->name,
		    "' died because of an uncaught exception"); };

	struct Guard
	{
		decltype(on_exception) &fn;
		bool ok = false;
		~Guard() { if (!ok) fn(); }
	} guard { .fn = on_exception };

	Thread::myself()->entry();
	guard.ok = true;

	Thread::myself()->_join.wakeup();

	/* sleep silently */
	Genode::sleep_forever();
}


/*****************
 ** Thread base **
 *****************/

void Thread::_init_native_thread(Stack &stack)
{
	Native_thread &nt = stack.native_thread();

	/*
	 * Allocate capability selectors for the thread's execution context,
	 * running semaphore and exception handler portals.
	 */

	/*
	 * Revoke possible left-over UTCB of a previously destroyed thread
	 * which used this context location.
	 *
	 * This cannot be done in '_deinit_platform_thread()', because a
	 * self-destructing thread needs its UTCB to call
	 * 'Cpu_session::kill_thread()' and is not able to revoke the UTCB
	 * afterwards.
	 */
	Nova::Rights rwx(true, true, true);
	addr_t utcb = reinterpret_cast<addr_t>(&stack.utcb());
	Nova::revoke(Nova::Mem_crd(utcb >> 12, 0, rwx));

	nt.exc_pt_sel = cap_map().insert(Nova::NUM_INITIAL_PT_LOG2);
	if (nt.exc_pt_sel == Native_thread::INVALID_INDEX) {
		error("failed allocate exception-portal selector for new thread");
		return;
	}

	_init_trace_control();

	/* create thread at core */
	_runtime.cpu.create_thread(_runtime.pd.rpc_cap(), name, _affinity, 0).with_result(
		[&] (Thread_capability cap) { _thread_cap = cap; },
		[&] (Cpu_session::Create_thread_error) {
			error("failed to create new thread for local PD"); });
}


void Thread::_init_native_main_thread(Stack &stack)
{
	Native_thread &nt = stack.native_thread();

	_thread_cap = _runtime.parent.main_thread_cap();

	nt.exc_pt_sel = 0;
	nt.ec_sel     = Nova::EC_SEL_THREAD;

	request_native_ec_cap(Nova::PT_SEL_PAGE_FAULT, nt.ec_sel);
	return;
}


void Thread::_deinit_native_thread(Stack &stack)
{
	Native_thread &nt = stack.native_thread();

	if (nt.ec_valid())
		Nova::revoke(Nova::Obj_crd(nt.ec_sel, 0));

	/* de-announce thread */
	_thread_cap.with_result(
		[&] (Thread_capability cap) { _runtime.cpu.kill_thread(cap); },
		[&] (Cpu_session::Create_thread_error) { });

	cap_map().remove(nt.exc_pt_sel, Nova::NUM_INITIAL_PT_LOG2);
}


Thread::Start_result Thread::start()
{
	return _stack.convert<Start_result>([&] (Stack &stack) {

		Native_thread &nt = stack.native_thread();

		if (nt.ec_sel < Native_thread::INVALID_INDEX - 1) {
			error("Thread::start failed due to invalid exception portal selector");
			return Start_result::DENIED;
		}

		if (_thread_cap.failed())
			return Start_result::DENIED;

		/*
		 * Default: create global thread - ec.sel == INVALID_INDEX
		 *          create  local thread - ec.sel == INVALID_INDEX - 1
		 */
		bool global = nt.ec_sel == Native_thread::INVALID_INDEX;

		/* create EC at core */
		Cpu_session::Native_cpu::Thread_type thread_type;

		if (global)
			thread_type = Cpu_session::Native_cpu::Thread_type::GLOBAL;
		else
			thread_type = Cpu_session::Native_cpu::Thread_type::LOCAL;

		Cpu_session::Native_cpu::Exception_base exception_base { nt.exc_pt_sel };

		Nova_native_cpu_client native_cpu(_runtime.cpu.native_cpu());
		native_cpu.thread_type(cap(), thread_type, exception_base);

		/* local thread have no start instruction pointer - set via portal entry */
		addr_t thread_ip = global ? reinterpret_cast<addr_t>(_thread_start) : nt.initial_ip;

		Cpu_thread_client cpu_thread(cap());
		cpu_thread.start(thread_ip, stack.top());

		/* request native EC thread cap */
		nt.ec_sel = nt.exc_pt_sel + Nova::EC_SEL_THREAD;

		/*
		 * Requested ec cap that is used for recall and
		 * creation of portals (Native_pd::alloc_rpc_cap).
		 */
		request_native_ec_cap(nt.exc_pt_sel + Nova::PT_SEL_PAGE_FAULT,
		                      nt.ec_sel);

		/* default: we don't accept any mappings or translations */
		Nova::Utcb &utcb = *(Nova::Utcb *)Thread::utcb();
		utcb.crd_rcv = Nova::Obj_crd();
		utcb.crd_xlt = Nova::Obj_crd();

		if (global)
			/* request creation of SC to let thread run*/
			cpu_thread.resume();

		return Start_result::OK;

	}, [&] (Stack_error) { return Start_result::DENIED; });
}
