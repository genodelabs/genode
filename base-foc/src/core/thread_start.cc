/*
 * \brief  Fiasco.OC-specific implementation of core's startup Thread API.
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2006-05-03
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/sleep.h>

/* core includes */
#include <cap_session_component.h>
#include <platform.h>
#include <core_env.h>

namespace Fiasco {
#include <l4/sys/debugger.h>
#include <l4/sys/factory.h>
}

using namespace Genode;


void Thread_base::_deinit_platform_thread()
{
	PWRN("%s: not implemented yet!", __func__);
}


void Thread_base::start()
{
	using namespace Fiasco;

	/* create and start platform thread */
	Platform_thread *pt =
		new(platform()->core_mem_alloc()) Platform_thread(_context->name);

	platform_specific()->core_pd()->bind_thread(pt);
	_tid = pt->gate().remote;
	_thread_cap =
		reinterpret_cap_cast<Cpu_thread>(Native_capability(pt->thread().local));
	pt->pager(platform_specific()->core_pager());

	_context->utcb = pt->utcb();
	l4_utcb_tcr_u(pt->utcb())->user[UTCB_TCR_BADGE] = (unsigned long) pt->gate().local.idx();
	l4_utcb_tcr_u(pt->utcb())->user[UTCB_TCR_THREAD_OBJ] = (addr_t)this;

	pt->start((void *)_thread_start, _context->stack);
}


void Thread_base::cancel_blocking()
{
	/*
	 * Within core, we never need to unblock threads
	 */
}
