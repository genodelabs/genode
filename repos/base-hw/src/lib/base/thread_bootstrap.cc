/*
 * \brief  Implementations for the initialization of a thread
 * \author Martin stein
 * \author Stefan Kalkowski
 * \date   2013-02-15
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/env.h>
#include <base/sleep.h>

/* base-internal includes */
#include <base/internal/stack.h>
#include <base/internal/native_utcb.h>

/* base-hw includes */
#include <kernel/interface.h>

using namespace Genode;

namespace Hw {
	Ram_dataspace_capability _main_thread_utcb_ds;
	Untyped_capability       _main_thread_cap;
	Untyped_capability       _parent_cap;
}


/*****************************
 ** Startup library support **
 *****************************/

void prepare_init_main_thread()
{
	using namespace Genode;
	using namespace Hw;

	/*
	 * Make data from the startup info persistantly available by copying it
	 * before the UTCB gets polluted by the following function calls.
	 */
	Native_utcb * utcb = Thread::myself()->utcb();
	_parent_cap = utcb->cap_get(Native_utcb::PARENT);
	Untyped_capability ds_cap(utcb->cap_get(Native_utcb::UTCB_DATASPACE));
	_main_thread_utcb_ds = reinterpret_cap_cast<Ram_dataspace>(ds_cap);
	_main_thread_cap = utcb->cap_get(Native_utcb::THREAD_MYSELF);
}


void prepare_reinit_main_thread() { prepare_init_main_thread(); }


/************
 ** Thread **
 ************/

Native_utcb *Thread::utcb()
{
	if (this) { return &_stack->utcb(); }
	return utcb_main_thread();
}


void Thread::_thread_start()
{
	Thread::myself()->_thread_bootstrap();
	Thread::myself()->entry();
	Thread::myself()->_join_lock.unlock();
	Genode::sleep_forever();
}

void Thread::_thread_bootstrap() {
	native_thread().cap = myself()->utcb()->cap_get(Native_utcb::THREAD_MYSELF); }
