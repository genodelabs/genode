/*
 * \brief  Implementations for the initialization of a thread
 * \author Martin stein
 * \author Stefan Kalkowski
 * \date   2013-02-15
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/env.h>
#include <base/sleep.h>

/* base-internal includes */
#include <base/internal/globals.h>
#include <base/internal/stack.h>
#include <base/internal/native_utcb.h>
#include <base/internal/capability_space.h>

using namespace Genode;

namespace Hw {
	Ram_dataspace_capability _main_thread_utcb_ds;
	Untyped_capability       _main_thread_cap;
	Untyped_capability       _parent_cap;
}


/*****************************
 ** Startup library support **
 *****************************/

void Genode::prepare_init_main_thread()
{
	using namespace Hw;

	/*
	 * Make data from the startup info persistantly available by copying it
	 * before the UTCB gets polluted by the following function calls.
	 */
	Native_utcb * utcb = Thread::myself()->utcb();
	_parent_cap = Capability_space::import(utcb->cap_get(Native_utcb::PARENT));
	Kernel::ack_cap(Capability_space::capid(_parent_cap));

	Untyped_capability ds_cap =
		Capability_space::import(utcb->cap_get(Native_utcb::UTCB_DATASPACE));
	_main_thread_utcb_ds = reinterpret_cap_cast<Ram_dataspace>(ds_cap);
	Kernel::ack_cap(Capability_space::capid(_main_thread_utcb_ds));

	_main_thread_cap = Capability_space::import(utcb->cap_get(Native_utcb::THREAD_MYSELF));
	Kernel::ack_cap(Capability_space::capid(_main_thread_cap));
}


/************
 ** Thread **
 ************/

/* prevent the compiler from optimizing out the 'this' pointer check */
__attribute__((optimize("-fno-delete-null-pointer-checks")))
Native_utcb *Thread::utcb()
{
	if (this)
		return &_stack->utcb();

	return utcb_main_thread();
}


void Thread::_thread_start()
{
	Thread::myself()->_thread_bootstrap();
	Thread::myself()->entry();
	Thread::myself()->_join.wakeup();
	Genode::sleep_forever();
}


void Thread::_thread_bootstrap()
{
	Kernel::capid_t capid = myself()->utcb()->cap_get(Native_utcb::THREAD_MYSELF);
	native_thread().cap = Capability_space::import(capid);
	if (native_thread().cap.valid())
		Kernel::ack_cap(Capability_space::capid(native_thread().cap));
}
