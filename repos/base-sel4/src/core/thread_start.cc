/*
 * \brief  Implementation of Thread API interface for core
 * \author Norman Feske
 * \date   2015-05-01
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/printf.h>
#include <base/sleep.h>

/* core includes */
#include <platform.h>
#include <platform_thread.h>
#include <thread_sel4.h>

using namespace Genode;


void Thread_base::_init_platform_thread(size_t, Type type)
{
	addr_t const utcb_virt_addr = (addr_t)&_context->utcb;

	if (type == MAIN) {
		_tid.tcb_sel = seL4_CapInitThreadTCB;
		return;
	}

	Thread_info thread_info;
	thread_info.init(utcb_virt_addr);

	if (!map_local(thread_info.ipc_buffer_phys, utcb_virt_addr, 1)) {
		PERR("could not map IPC buffer phys %lx at local %lx",
		     thread_info.ipc_buffer_phys, utcb_virt_addr);
	}

	_tid.tcb_sel = thread_info.tcb_sel;
	_tid.ep_sel  = thread_info.ep_sel;

	Platform &platform = *platform_specific();

	seL4_CapData_t no_cap_data = { { 0 } };
	int const ret = seL4_TCB_SetSpace(_tid.tcb_sel, 0,
	                                  platform.top_cnode().sel(), no_cap_data,
	                                  seL4_CapInitThreadPD, no_cap_data);
	ASSERT(ret == 0);
}


void Thread_base::_deinit_platform_thread()
{
	PDBG("not implemented");
}


void Thread_base::_thread_start()
{
	Thread_base::myself()->_thread_bootstrap();
	Thread_base::myself()->entry();

	sleep_forever();
}


void Thread_base::start()
{
	start_sel4_thread(_tid.tcb_sel, (addr_t)&_thread_start, (addr_t)stack_top());
}


void Thread_base::cancel_blocking()
{
	PWRN("not implemented");
}

