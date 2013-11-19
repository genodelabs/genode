/**
 * \brief  Platform specific parts of the thread API
 * \author Martin Stein
 * \date   2012-02-12
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
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

extern Native_utcb * __initial_sp;

namespace Genode { Rm_session * env_context_area_rm_session(); }


/*****************
 ** Thread_base **
 *****************/

void Thread_base::_init_platform_thread() { }


Native_utcb * Thread_base::utcb()
{
	if (this) { return &_context->utcb; }
	return __initial_sp;
}


void Thread_base::_thread_start()
{
	Thread_base::myself()->_thread_bootstrap();
	Thread_base::myself()->entry();
	Thread_base::myself()->_join_lock.unlock();
	Genode::sleep_forever();
}


void Thread_base::_deinit_platform_thread()
{
	/* detach userland thread-context */
	size_t const size = sizeof(_context->utcb);
	addr_t utcb = Context_allocator::addr_to_base(_context) +
	              Native_config::context_virtual_size() - size -
	              Native_config::context_area_virtual_base();
	env_context_area_rm_session()->detach(utcb);

	/* destroy server object */
	env()->cpu_session()->kill_thread(_thread_cap);
	if (_pager_cap.valid()) {
		env()->rm_session()->remove_client(_pager_cap);
	}
}


void Thread_base::start()
{
	/* create server object */
	char buf[48];
	name(buf, sizeof(buf));
	Cpu_session * cpu = env()->cpu_session();
	_thread_cap = cpu->create_thread(buf, (addr_t)&_context->utcb);

	/* assign thread to protection domain */
	env()->pd_session()->bind_thread(_thread_cap);

	/* create pager object and assign it to the thread */
	_pager_cap = env()->rm_session()->add_client(_thread_cap);
	env()->cpu_session()->set_pager(_thread_cap, _pager_cap);

	/* attach userland thread-context */
	try {
		Ram_dataspace_capability ds = env()->cpu_session()->utcb(_thread_cap);
		size_t const size = sizeof(_context->utcb);
		addr_t dst = Context_allocator::addr_to_base(_context) +
		             Native_config::context_virtual_size() - size -
		             Native_config::context_area_virtual_base();
		env_context_area_rm_session()->attach_at(ds, dst, size);
	} catch (...) {
		PERR("failed to attach userland thread-context");
		sleep_forever();
	}
	/* start thread with its initial IP and aligned SP */
	addr_t thread_sp = (addr_t)&_context->stack[-4];
	thread_sp &= ~0xf;
	env()->cpu_session()->start(_thread_cap, (addr_t)_thread_start, thread_sp);
}


void Thread_base::cancel_blocking()
{
	env()->cpu_session()->cancel_blocking(_thread_cap);
}
