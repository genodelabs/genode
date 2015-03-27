/*
 * \brief  Fiasco-specific implementation of the non-core startup Thread API
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \author Martin Stein
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

namespace Fiasco {
#include <l4/sys/utcb.h>
}

using namespace Genode;


void Thread_base::_deinit_platform_thread()
{
	using namespace Fiasco;

	if (_context->utcb && _thread_cap.valid()) {
		Cap_index *i = (Cap_index*)l4_utcb_tcr_u(_context->utcb)->user[UTCB_TCR_BADGE];
		cap_map()->remove(i);
		_cpu_session->kill_thread(_thread_cap);
		env()->rm_session()->remove_client(_pager_cap);
	}
}


void Thread_base::_init_platform_thread(size_t weight, Type type)
{
	/* if no cpu session is given, use it from the environment */
	if (!_cpu_session)
		_cpu_session = env()->cpu_session();

	if (type == NORMAL)
	{
		/* create thread at core */
		char buf[48];
		name(buf, sizeof(buf));
		_thread_cap = _cpu_session->create_thread(weight, buf);

		/* assign thread to protection domain */
		if (!_thread_cap.valid() ||
		    env()->pd_session()->bind_thread(_thread_cap))
			throw Cpu_session::Thread_creation_failed();
		return;
	}
	/* adjust values whose computation differs for a main thread */
	_tid = Fiasco::MAIN_THREAD_CAP;
	_thread_cap = env()->parent()->main_thread_cap();

	if (!_thread_cap.valid())
		throw Cpu_session::Thread_creation_failed();

	/* make thread object known to the Fiasco environment */
	addr_t const t = (addr_t)this;
	Fiasco::l4_utcb_tcr()->user[Fiasco::UTCB_TCR_THREAD_OBJ] = t;
}


void Thread_base::start()
{
	using namespace Fiasco;

	/* create new pager object and assign it to the new thread */
	_pager_cap = env()->rm_session()->add_client(_thread_cap);
	_cpu_session->set_pager(_thread_cap, _pager_cap);

	/* get gate-capability and badge of new thread */
	Thread_state state;
	try { state = _cpu_session->state(_thread_cap); }
	catch (...) { throw Cpu_session::Thread_creation_failed(); }
	_tid = state.kcap;
	_context->utcb = state.utcb;

	Cap_index *i = cap_map()->insert(state.id, state.kcap);
	l4_utcb_tcr_u(state.utcb)->user[UTCB_TCR_BADGE]      = (unsigned long) i;
	l4_utcb_tcr_u(state.utcb)->user[UTCB_TCR_THREAD_OBJ] = (addr_t)this;

	/* register initial IP and SP at core */
	_cpu_session->start(_thread_cap, (addr_t)_thread_start, _context->stack_top());
}


void Thread_base::cancel_blocking()
{
	_cpu_session->cancel_blocking(_thread_cap);
}
