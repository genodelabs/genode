/*
 * \brief  Implementation of Thread API for roottask
 * \author Norman Feske
 * \date   2010-09-01
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <base/thread.h>
#include <base/sleep.h>
#include <base/native_types.h>
#include <platform.h>
#include <platform_pd.h>
#include <kernel/syscalls.h>

using namespace Genode;

static bool const verbose = 0;

namespace Roottask {

	Platform_pd* platform_pd()
	{
		static Platform_pd _pd(PROTECTION_ID);
		return &_pd;
	}
}


void Thread_base::_init_platform_thread() { }


void Thread_base::_deinit_platform_thread()
{
	Platform_pd *pd = Roottask::platform_pd();
	Platform_pd::Context_id cid;

	if (pd->cid_if_context_address((addr_t)_context, &cid)){
		pd->free_context(cid);
	}

	if(Kernel::thread_kill(_tid)){
		PERR("Kernel::thread_kill(%i) failed", (unsigned)_tid);
	}
	tid_allocator()->free(_tid);
}


void Thread_base::_thread_start()
{
	myself()->entry();
	PDBG("Thread returned, tid=%i, pid=%i",
	     myself()->tid(), Roottask::PROTECTION_ID);

	Genode::sleep_forever();
}


void Thread_base::_init_context(Context* context)
{
	_tid=tid_allocator()->allocate();
	Platform_pd *pd = Roottask::platform_pd();

	Platform_pd::Context_id cid;
	if (!pd->cid_if_context_address((addr_t)context, &cid)){
		PERR("Invalid context address 0x%p", context);
		return;
	}
	if (!pd->allocate_context(_tid, cid)){
		PERR("Allocating context %i failed", cid);
		return;
	}
}


void Thread_base::start()
{
	using namespace Genode;

	Native_process_id  const       pid = Roottask::PROTECTION_ID;
	Native_thread_id   const pager_tid = Roottask::PAGER_TID;
	void             * const       vsp = &_context->stack;
	Native_utcb      * const     vutcb = &_context->utcb;
	Kernel::Utcb     * const     putcb = physical_utcb(_tid);
	void             * const       vip = (void *)_thread_start;

	if(verbose) {
		PDBG("Start Thread, tid=%i, pid=%i, pager=%i", _tid, pid, pager_tid);
		PDBG("   vip=0x%p, vsp=%p, vutcb=0x%p", vip, vsp, vutcb);
		PDBG("   pip=0x%p, psp=%p, putcb=0x%p", 
		     vip, (void*)Roottask::physical_context(_tid)->stack, putcb);
	}

	if (Kernel::thread_create(_tid, pid, pager_tid,
	                          putcb, (addr_t)vip, (addr_t)vsp,
	                          1 << Kernel::THREAD_CREATE__PARAM__IS_ROOT_LSHIFT))
	{
		PERR("Kernel::thread_create failed");
	}
}


void Thread_base::cancel_blocking() { PERR("not implemented"); }

