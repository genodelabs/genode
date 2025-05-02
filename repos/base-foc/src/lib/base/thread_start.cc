/*
 * \brief  Fiasco.OC-specific implementation of the non-core startup Thread API
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \author Martin Stein
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
#include <base/env.h>
#include <cpu_thread/client.h>
#include <foc/native_capability.h>
#include <foc_native_cpu/client.h>

/* base-internal includes */
#include <base/internal/stack.h>
#include <base/internal/cap_map.h>
#include <base/internal/globals.h>

/* Fiasco.OC includes */
#include <foc/syscall.h>

using namespace Genode;


static Capability<Pd_session> pd_session_cap(Capability<Pd_session> pd_cap = { })
{
	static Capability<Pd_session> cap = pd_cap; /* defined once by 'init_thread_start' */
	return cap;
}


static Thread_capability main_thread_cap(Thread_capability main_cap = { })
{
	static Thread_capability cap = main_cap; /* defined once by 'init_thread_bootstrap' */
	return cap;
}


void Thread::_deinit_native_thread(Stack &stack)
{
	using namespace Foc;

	if (stack.native_thread().kcap) {
		Cap_index *i = (Cap_index*)l4_utcb_tcr_u(utcb()->foc_utcb)->user[UTCB_TCR_BADGE];
		cap_map().remove(i);
	}

	_thread_cap.with_result(
		[&] (Thread_capability cap) { _cpu_session->kill_thread(cap); },
		[&] (Cpu_session::Create_thread_error) { });
}


void Thread::_init_native_thread(Stack &stack, size_t weight, Type type)
{
	_init_cpu_session_and_trace_control();

	if (type == NORMAL) {

		/* create thread at core */
		_thread_cap = _cpu_session->create_thread(pd_session_cap(), name,
		                                          _affinity, Weight(weight));
		return;
	}

	/* adjust values whose computation differs for a main thread */
	stack.native_thread().kcap = Foc::MAIN_THREAD_CAP;
	_thread_cap = main_thread_cap();

	if (_thread_cap.failed()) {
		error("failed to re-initialize main thread");
		return;
	}

	/* make thread object known to the Fiasco.OC environment */
	addr_t const t = (addr_t)this;
	Foc::l4_utcb_tcr()->user[Foc::UTCB_TCR_THREAD_OBJ] = t;
}


Thread::Start_result Thread::start()
{
	using namespace Foc;

	return _thread_cap.convert<Start_result>(
		[&] (Thread_capability cap) {
			Foc_native_cpu_client native_cpu(_cpu_session->native_cpu());

			/* get gate-capability and badge of new thread */
			Foc_thread_state state { };
			state = native_cpu.thread_state(cap);

			/* remember UTCB of the new thread */
			Foc::l4_utcb_t * const foc_utcb = (Foc::l4_utcb_t *)state.utcb;
			utcb()->foc_utcb = foc_utcb;

			with_native_thread([&] (Native_thread &nt) {
				nt.kcap = state.kcap; });

			Cap_index *i = cap_map().insert(state.id, state.kcap);
			l4_utcb_tcr_u(foc_utcb)->user[UTCB_TCR_BADGE]      = (unsigned long) i;
			l4_utcb_tcr_u(foc_utcb)->user[UTCB_TCR_THREAD_OBJ] = (addr_t)this;

			/* register initial IP and SP at core */
			Cpu_thread_client cpu_thread(cap);

			return _stack.convert<Start_result>(
				[&] (Stack &stack) {
					cpu_thread.start((addr_t)_thread_start, stack.top());
					return Start_result::OK;
				},
				[&] (Stack_error) { return Start_result::DENIED; });
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
