/*
 * \brief  NOVA-specific implementation of the Thread API for core
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

/* core includes */
#include <nova_util.h>

using namespace Genode;


/**
 * This function is called for constructing server activations and pager
 * objects. It allocates capability selectors for the thread's execution
 * context and a synchronization-helper semaphore needed for 'Lock'.
 */
void Thread_base::_init_platform_thread()
{
	_tid.ec_sel = cap_selector_allocator()->alloc();
	_tid.sc_sel = ~0; /* not needed within core */
	_tid.rs_sel = cap_selector_allocator()->alloc();
	_tid.pd_sel = cap_selector_allocator()->pd_sel();
	_tid.exc_pt_sel = cap_selector_allocator()->alloc(Nova::NUM_INITIAL_PT_LOG2);

	/* create running semaphore required for locking */
	uint8_t res = Nova::create_sm(_tid.rs_sel, _tid.pd_sel, 0);
	if (res)
		PERR("create_sm returned %u", res);
}


void Thread_base::_deinit_platform_thread()
{
	unmap_local(Nova::Obj_crd(_tid.ec_sel, 0));
	unmap_local(Nova::Obj_crd(_tid.rs_sel, 0));
	unmap_local(Nova::Obj_crd(_tid.exc_pt_sel, Nova::NUM_INITIAL_PT_LOG2));

	cap_selector_allocator()->free(_tid.ec_sel, 0);
	cap_selector_allocator()->free(_tid.rs_sel, 0);
	cap_selector_allocator()->free(_tid.exc_pt_sel, Nova::NUM_INITIAL_PT_LOG2);

	/* revoke utcb */
	Nova::Rights rwx(true, true, true);
	addr_t utcb = reinterpret_cast<addr_t>(&_context->utcb);
	Nova::revoke(Nova::Mem_crd(utcb >> 12, 0, rwx));
}


void Thread_base::start()
{
	/*
	 * On NOVA, core never starts regular threads.
	 */
}

void Thread_base::cancel_blocking()
{
	Nova::sm_ctrl(_tid.rs_sel, Nova::SEMAPHORE_UP);
}
