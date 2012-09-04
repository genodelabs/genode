/*
 * \brief  Thread facility
 * \author Norman Feske
 * \author Martin Stein
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <base/ipc_pager.h>
#include <platform_thread.h>
#include <platform_pd.h>
#include <kernel/syscalls.h>
#include "include/platform.h"

static bool const verbose = 0;

using namespace Genode;


Tid_allocator* Genode::tid_allocator()
{
	static Tid_allocator _tida(User::MIN_THREAD_ID+1, User::MAX_THREAD_ID);
	return &_tida;
}


Pid_allocator* Genode::pid_allocator()
{
	static Pid_allocator _pida(User::MIN_PROTECTION_ID, User::MAX_PROTECTION_ID);
	return &_pida;
}


namespace Genode 
{
	static Kernel::Utcb * phys_utcb[User::MAX_THREAD_ID];
}


int Genode::physical_utcb(Native_thread_id const &tid, Kernel::Utcb * const &utcb)
{
	if (tid < sizeof(phys_utcb)/sizeof(phys_utcb[0])) {
		Genode::phys_utcb[tid] = utcb;
		return true;
	}
	return false;
}


Kernel::Utcb* Genode::physical_utcb(Native_thread_id tid)
{

	if (tid >= sizeof(phys_utcb)/sizeof(phys_utcb[0])) {
		PERR("Native thread ID out of range");
		return 0;
	}

	if(!phys_utcb[tid]) {
		if (!platform_specific()->
		           core_mem_alloc()->
		           alloc_aligned(sizeof(Kernel::Utcb), 
		                         (void**)&phys_utcb[tid], 
		                         Kernel::Utcb::ALIGNMENT_LOG2))
		{
			PERR("Allocate memory for a new UTCB failed");
			return 0;
		}
		if(verbose) {
			PDBG("UTCB %i: [%p|%p]", tid, phys_utcb[tid], 
			     (void*)((addr_t)phys_utcb[tid] + sizeof(Kernel::Utcb)));
		}
	}
	return phys_utcb[tid];
}


void Platform_thread::affinity(unsigned int cpu_no) { PERR("not implemented"); }


void Platform_thread::cancel_blocking() { PERR("not implemented"); }


int Platform_thread::state(Thread_state *state_dst)
{
	PERR("not implemented");
	return -1;
}


int Platform_thread::start(void *ip, void *sp, unsigned int cpu_no)
{
	Native_thread_id pager_tid = _pager ? _pager->cap().dst() : 0;
	Kernel::Utcb* putcb        = physical_utcb(_tid);

	/* Hand over arguments for the thread's bootstrap */
	*((Native_thread_id*)&putcb->byte[0]) = _tid;
	if (verbose) {
		PDBG("Start Thread, tid=%i, pid=%i, pager=%i", _tid, _pid, pager_tid);
		PDBG("vip=0x%p, vsp=%p, vutcb=0x%p", ip, sp, _utcb);
	}
	int const error = Kernel::thread_create(_tid, _pid, pager_tid,
	                                        putcb, (addr_t)ip, (addr_t)sp, 0);
	if (error) {
		PERR("Kernel::thread_create failed, error=%di", error);
		return -1;
	}
	return 0;
}


void Platform_thread::pause()
{
	PDBG("not implemented");
}


void Platform_thread::resume()
{
	PDBG("not implemented");
}


Platform_thread::Platform_thread(const char *name, unsigned, int thread_id,
                                 uint32_t params)
: _tid(0), _utcb(0), _params(params)
{
	if (!_tid) {
		if (!(_tid = tid_allocator()->allocate(this))) {
			PERR("TID allocation failed");
			return;
		}
	}
	else if (!tid_allocator()->allocate(this, (Native_thread_id)thread_id)) {
		PERR("TID allocation failed");
		return;
	}
}


Platform_thread::~Platform_thread() { 
	_pd->unbind_thread(this);
	if(Kernel::thread_kill(_tid)){
		PERR("Kernel::thread_kill(%i) failed", (unsigned)_tid);
	}
	tid_allocator()->free(_tid);
}


bool Ipc_pager::resolved()
{
	typedef Platform_pd::Context_part Context_part;
	typedef Mapping::Physical_page    Physical_page;

	addr_t           va            = (addr_t)_request.virtual_page.address();
	Native_thread_id context_owner = 0;
	Context_part     context_part  = Platform_pd::NO_CONTEXT_PART;
	unsigned         stack_offset  = 0;

	Platform_pd* pd =
		pid_allocator()->holder(_request.virtual_page.protection_id());

	if (!pd) { return false; }
	if (!pd->metadata_if_context_address(va, &context_owner, &context_part,
	                                     &stack_offset)) 
	{
		return false;
	}

	if (context_part != Platform_pd::UTCB_AREA) { return false; }

	Native_utcb * const putcb = physical_utcb(context_owner);
	set_reply_mapping(Genode::Mapping(va, (addr_t)putcb, false, 
	                                  Native_utcb::size_log2(), true));

	return true;
}
