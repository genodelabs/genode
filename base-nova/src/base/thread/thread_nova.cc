/*
 * \brief  NOVA-specific implementation of the Thread API
 * \author Norman Feske
 * \author Sebastian Sumpf
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

/* NOVA includes */
#include <nova/syscalls.h>

using namespace Genode;


/**
 * Entry point entered by new threads
 */
void Thread_base::_thread_start()
{
	Genode::Thread_base::myself()->entry();
	Genode::sleep_forever();
}


static void request_event_portal(Pager_capability pager_cap,
                                 int exc_base, int event)
{
	using namespace Nova;
	Utcb *utcb = (Utcb *)Thread_base::myself()->utcb();

	/* save original receive window */
	Crd orig_crd = utcb->crd_rcv;

	/* request event-handler portal */
	utcb->msg[0] = event;
	utcb->set_msg_word(1);
	utcb->crd_rcv = Obj_crd(exc_base + event, 0);

	int res = call(pager_cap.dst());
	if (res)
		PERR("request of event (%d) capability selector failed", event);

	/* restore original receive window */
	utcb->crd_rcv = orig_crd;
}


/*****************
 ** Thread base **
 *****************/

void Thread_base::_init_platform_thread()
{
	using namespace Nova;

	/*
	 * Allocate capability selectors for the thread's execution context,
	 * scheduling context, running semaphore and exception handler portals.
	 */
	_tid.ec_sel     = cap_selector_allocator()->alloc();
	_tid.sc_sel     = cap_selector_allocator()->alloc();
	_tid.rs_sel     = cap_selector_allocator()->alloc();
	_tid.pd_sel     = cap_selector_allocator()->pd_sel();
	_tid.exc_pt_sel = cap_selector_allocator()->alloc(NUM_INITIAL_PT_LOG2);

	/* create thread at core */
	char buf[48];
	name(buf, sizeof(buf));
	Thread_capability thread_cap = env()->cpu_session()->create_thread(buf);

	/* create new pager object and assign it to the new thread */
	Pager_capability pager_cap = env()->rm_session()->add_client(thread_cap);
	env()->cpu_session()->set_pager(thread_cap, pager_cap);

	/* register initial IP and SP at core */
	mword_t thread_sp = (mword_t)&_context->stack[-4];
	env()->cpu_session()->start(thread_cap, (addr_t)_thread_start, thread_sp);

	request_event_portal(pager_cap, _tid.exc_pt_sel, PT_SEL_STARTUP);
	request_event_portal(pager_cap, _tid.exc_pt_sel, PT_SEL_PAGE_FAULT);

	/* create running semaphore required for locking */
	int res = create_sm(_tid.rs_sel, _tid.pd_sel, 0);
	if (res)
		PERR("create_sm returned %d", res);
}


void Thread_base::_deinit_platform_thread()
{
	Nova::revoke(Nova::Obj_crd(_tid.sc_sel, 0));
	Nova::revoke(Nova::Obj_crd(_tid.ec_sel, 0));
	Nova::revoke(Nova::Obj_crd(_tid.rs_sel, 0));

	/* revoke utcb */
	Nova::Rights rwx(true, true, true);
	addr_t utcb = reinterpret_cast<addr_t>(&_context->utcb);
	Nova::revoke(Nova::Mem_crd(utcb >> 12, 0, rwx));
}


void Thread_base::start()
{
	using namespace Nova;

	/* create execution context */
	enum { THREAD_CPU_NO = 0, THREAD_GLOBAL  = true };
	int res = create_ec(_tid.ec_sel, _tid.pd_sel, THREAD_CPU_NO, (mword_t)&_context->utcb,
	                    0, _tid.exc_pt_sel, THREAD_GLOBAL);
	if (res)
		PDBG("create_ec returned %d", res);

	/*
	 * Create scheduling context
	 *
	 * With assigning a scheduling context to the execution context, the new
	 * thread will immediately start, enter the startup portal, and receives
	 * the configured initial IP and SP from core.
	 */
	res = create_sc(_tid.sc_sel, _tid.pd_sel, _tid.ec_sel, Qpd());
	if (res)
		PERR("create_sc returned %d", res);
}


void Thread_base::cancel_blocking()
{
	Nova::sm_ctrl(_tid.rs_sel, Nova::SEMAPHORE_UP);
}
