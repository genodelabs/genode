/**
 * \brief  Platform specific parts of the thread API
 * \author Martin Stein
 * \date   2012-02-12
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <base/env.h>

using namespace Genode;

extern Native_utcb * _main_utcb;

namespace Genode { Rm_session  *env_context_area_rm_session(); }


/*****************
 ** Thread_base **
 *****************/

Native_utcb * Thread_base::utcb()
{
	/* this is a main thread, so CRT0 provides UTCB through '_main_utcb' */
	if (!this) return _main_utcb;

	/* otherwise we have a valid thread base */
	return &_context->utcb;
}


void Thread_base::_thread_start()
{
	Thread_base::myself()->_thread_bootstrap();
	Thread_base::myself()->entry();
	Genode::sleep_forever();
}


void Thread_base::_init_platform_thread() { }


void Thread_base::_deinit_platform_thread()
{ env()->cpu_session()->kill_thread(_thread_cap); }


void Thread_base::start()
{
	/* create thread at core */
	char buf[48];
	name(buf, sizeof(buf));
	Cpu_session * cpu = env()->cpu_session();
	_thread_cap = cpu->create_thread(buf, (addr_t)&_context->utcb);

	/* assign thread to protection domain */
	env()->pd_session()->bind_thread(_thread_cap);

	/* create new pager object and assign it to the new thread */
	Pager_capability pager_cap = env()->rm_session()->add_client(_thread_cap);
	env()->cpu_session()->set_pager(_thread_cap, pager_cap);

	/* attach UTCB */
	try {
		Ram_dataspace_capability ds = env()->cpu_session()->utcb(_thread_cap);
		size_t const size = sizeof(_context->utcb);
		addr_t dst = Context_allocator::addr_to_base(_context) +
		             CONTEXT_VIRTUAL_SIZE - size - CONTEXT_AREA_VIRTUAL_BASE;
		env_context_area_rm_session()->attach_at(ds, dst, size);
	} catch (...) {
		PERR("%s: Failed to attach UTCB", __PRETTY_FUNCTION__);
		sleep_forever();
	}
	/* start thread with its initial IP and aligned SP */
	addr_t thread_sp = (addr_t)&_context->stack[-4];
	thread_sp &= ~0xf;
	env()->cpu_session()->start(_thread_cap, (addr_t)_thread_start, thread_sp);
}


void Thread_base::cancel_blocking()
{ env()->cpu_session()->cancel_blocking(_thread_cap); }

