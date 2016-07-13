/*
 * \brief   Fiasco thread facility
 * \author  Christian Helmuth
 * \date    2006-04-11
 *
 * This provides a thread object and uses l4_inter_task_ex_regs() for L4 thread
 * manipulation.
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <util/string.h>
#include <cpu_session/cpu_session.h>

/* core includes */
#include <platform_thread.h>

/* base-internal includes */
#include <base/internal/capability_space_tpl.h>

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/types.h>
#include <l4/sys/syscalls.h>
#include <l4/sys/utcb.h>
#include <l4/sys/kdebug.h>
}

using namespace Genode;
using namespace Fiasco;


int Platform_thread::start(void *ip, void *sp)
{
	l4_umword_t dummy, old_eflags;
	l4_threadid_t thread      = _l4_thread_id;
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
		warning("old eflags == ~0 on ex_regs ",
		        Hex(thread.id.task), ".", Hex(thread.id.lthread));

	fiasco_register_thread_name(thread, _name);
	return 0;
}


void Platform_thread::pause()
{
	warning(__func__, " not implemented");
}


void Platform_thread::resume()
{
	warning(__func__, " not implemented");
}


void Platform_thread::bind(int thread_id, l4_threadid_t l4_thread_id, Platform_pd *pd)
{
	_thread_id    = thread_id;
	_l4_thread_id = l4_thread_id;
	_platform_pd  = pd;
}


void Platform_thread::unbind()
{
	l4_umword_t dummy, old_eflags;
	l4_threadid_t thread      = _l4_thread_id;
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
		warning("old eflags == ~0 on ex_regs ",
		        Hex(thread.id.task), ".", Hex(thread.id.lthread));

	_thread_id    = THREAD_INVALID;
	_l4_thread_id = L4_INVALID_ID;
	_platform_pd  = 0;
}


void Platform_thread::state(Thread_state s)
{
	warning(__func__, " not implemented");
	throw Cpu_thread::State_access_failed();
}


Thread_state Platform_thread::state()
{
	Thread_state s;

	l4_umword_t   old_eflags, ip, sp;
	l4_threadid_t thread      = _l4_thread_id;
	l4_threadid_t pager       = L4_INVALID_ID;
	l4_threadid_t preempter   = L4_INVALID_ID;
	l4_threadid_t cap_handler = L4_INVALID_ID;

	l4_inter_task_ex_regs(thread, ~0UL, ~0UL,
	                      &preempter, &pager, &cap_handler,
	                      &old_eflags, &ip, &sp,
	                      L4_THREAD_EX_REGS_NO_CANCEL, l4_utcb_get());
	if (old_eflags == ~0UL)
		warning("old eflags == ~0 on ex_regs ",
		        Hex(thread.id.task), ".", Hex(thread.id.lthread));

	/* fill thread state structure */
	s.ip = ip;
	s.sp = sp;

	return s;
}


void Platform_thread::cancel_blocking()
{
	l4_umword_t   dummy;
	l4_threadid_t invalid = L4_INVALID_ID;

	l4_inter_task_ex_regs(_l4_thread_id, ~0UL, ~0UL,
	                      &invalid, &invalid, &invalid,
	                      &dummy, &dummy, &dummy, 0, l4_utcb_get());
}


Weak_ptr<Address_space> Platform_thread::address_space()
{
	return _platform_pd->Address_space::weak_ptr();
}


Platform_thread::Platform_thread(size_t, const char *name, unsigned,
                                 Affinity::Location, addr_t,
                                 int thread_id)
: _thread_id(thread_id), _l4_thread_id(L4_INVALID_ID), _pager(0)
{
	strncpy(_name, name, sizeof(_name));
}


Platform_thread::~Platform_thread()
{
	/*
	 * We inform our protection domain about thread destruction, which will end up in
	 * Thread::unbind()
	 */
	if (_platform_pd)
		_platform_pd->unbind_thread(this);
}
