/*
 * \brief  Lock implementation
 * \author Norman Feske
 * \date   2007-10-15
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/cancelable_lock.h>
#include <cpu/atomic.h>
#include <cpu/memory_barrier.h>

/* L4/Fiasco includes */
namespace Fiasco {
#include <l4/sys/ipc.h>
}

using namespace Genode;


Cancelable_lock::Cancelable_lock(Cancelable_lock::State initial)
: _state(UNLOCKED), _owner(nullptr)
{
	if (initial == LOCKED)
		lock();
}


void Cancelable_lock::lock()
{
	/*
	 * XXX: How to notice cancel-blocking signals issued when  being outside the
	 *      'l4_ipc_sleep' system call?
	 */
	while (!Genode::cmpxchg(&_state, UNLOCKED, LOCKED))
		if (Fiasco::l4_ipc_sleep(Fiasco::l4_ipc_timeout(0, 0, 500, 0)) != L4_IPC_RETIMEOUT)
			throw Genode::Blocking_canceled();
}


void Cancelable_lock::unlock()
{
	Genode::memory_barrier();
	_state = UNLOCKED;
}
