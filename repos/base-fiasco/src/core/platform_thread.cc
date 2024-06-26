/*
 * \brief   Fiasco thread facility
 * \author  Christian Helmuth
 * \date    2006-04-11
 *
 * This provides a thread object and uses l4_inter_task_ex_regs() for L4 thread
 * manipulation.
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <cpu_session/cpu_session.h>

/* core includes */
#include <platform_thread.h>

/* base-internal includes */
#include <base/internal/capability_space_tpl.h>

/* L4/Fiasco includes */
#include <fiasco/syscall.h>

using namespace Core;
using namespace Fiasco;


void Platform_thread::start(void *ip, void *sp)
{
	if (_id.failed())
		 return;

	l4_umword_t dummy, old_eflags;
	l4_threadid_t thread      = native_thread_id();
	l4_threadid_t pager       = _pager
	                          ? Capability_space::ipc_cap_data(_pager->cap()).dst
	                          : L4_INVALID_ID;
	l4_threadid_t preempter   = L4_INVALID_ID;
	l4_threadid_t cap_handler = L4_INVALID_ID;

	l4_inter_task_ex_regs(thread, (l4_umword_t)ip, (l4_umword_t)sp,
	                      &preempter, &pager, &cap_handler,
	                      &old_eflags, &dummy, &dummy,
	                      0, l4_utcb_get());
	if (old_eflags == ~0UL)
		warning("start ", _name, ": old eflags == ~0 on ex_regs ",
		        Hex(thread.id.task), ".", Hex(thread.id.lthread));

	fiasco_register_thread_name(thread, _name.string());
}


Thread_state Platform_thread::state()
{
	Thread_state s { };

	l4_umword_t   old_eflags, ip, sp;
	l4_threadid_t thread      = native_thread_id();
	l4_threadid_t pager       = L4_INVALID_ID;
	l4_threadid_t preempter   = L4_INVALID_ID;
	l4_threadid_t cap_handler = L4_INVALID_ID;

	l4_inter_task_ex_regs(thread, ~0UL, ~0UL,
	                      &preempter, &pager, &cap_handler,
	                      &old_eflags, &ip, &sp,
	                      L4_THREAD_EX_REGS_NO_CANCEL, l4_utcb_get());
	if (old_eflags == ~0UL)
		warning("state old eflags == ~0 on ex_regs ",
		        Hex(thread.id.task), ".", Hex(thread.id.lthread));

	/* fill thread state structure */
	s.cpu.ip = ip;
	s.cpu.sp = sp;
	s.state  = Thread_state::State::VALID;

	return s;
}


Platform_thread::~Platform_thread()
{
	if (_id.failed())
		return;

	l4_umword_t dummy, old_eflags;
	l4_threadid_t thread      = native_thread_id();
	l4_threadid_t pager       = thread;
	l4_threadid_t preempter   = L4_INVALID_ID;
	l4_threadid_t cap_handler = L4_INVALID_ID;

	fiasco_register_thread_name(thread, "<dead>");

	/*
	 * The Fiasco thread is halted by setting itself as pager and forcing
	 * pagefault at 0, where Genode never maps a page. The bottom line is the
	 * thread blocks in IPC to itself.
	 */
	l4_inter_task_ex_regs(thread, 0, 0,
	                      &preempter, &pager, &cap_handler,
	                      &old_eflags, &dummy, &dummy,
	                      0, l4_utcb_get());
	if (old_eflags == ~0UL)
		warning("unbind old eflags == ~0 on ex_regs ",
		        Hex(thread.id.task), ".", Hex(thread.id.lthread));

	_id.with_result(
		[&] (Platform_pd::Thread_id id) { _pd.free_thread_id(id); },
		[&] (Platform_pd::Alloc_thread_id_error) { });
}
