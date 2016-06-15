/*
 * \brief  Fiasco.OC-specific implementation of core's startup Thread API.
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2006-05-03
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/sleep.h>

/* base-internal includes */
#include <base/internal/stack.h>

/* core includes */
#include <platform.h>
#include <core_env.h>

namespace Fiasco {
#include <l4/sys/debugger.h>
#include <l4/sys/factory.h>
}

using namespace Genode;


void Thread::_deinit_platform_thread()
{
	PWRN("%s: not implemented yet!", __func__);
}


void Thread::_init_platform_thread(size_t, Type) { }


void Thread::start()
{
	using namespace Fiasco;

	/* create and start platform thread */
	Platform_thread *pt =
		new(platform()->core_mem_alloc()) Platform_thread(_stack->name().string());

	platform_specific()->core_pd()->bind_thread(pt);

	l4_utcb_t *foc_utcb = (l4_utcb_t *)(pt->utcb());

	native_thread() = Native_thread(pt->gate().remote);

	utcb()->foc_utcb = foc_utcb;

	_thread_cap =
		reinterpret_cap_cast<Cpu_thread>(Native_capability(pt->thread().local));

	pt->pager(platform_specific()->core_pager());

	l4_utcb_tcr_u(foc_utcb)->user[UTCB_TCR_BADGE] = (unsigned long) pt->gate().local.data();
	l4_utcb_tcr_u(foc_utcb)->user[UTCB_TCR_THREAD_OBJ] = (addr_t)this;

	pt->start((void *)_thread_start, stack_top());
}


void Thread::cancel_blocking()
{
	/*
	 * Within core, we never need to unblock threads
	 */
}
