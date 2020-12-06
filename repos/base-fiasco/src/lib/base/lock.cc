/*
 * \brief  Lock implementation
 * \author Norman Feske
 * \date   2007-10-15
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread.h>
#include <cpu/atomic.h>
#include <cpu/memory_barrier.h>

/* L4/Fiasco includes */
#include <fiasco/syscall.h>

using namespace Genode;


Lock::Lock(Lock::State initial)
:
	_state(UNLOCKED), _owner(nullptr)
{
	if (initial == LOCKED)
		lock();
}


void Lock::lock()
{
	Applicant myself(Thread::myself());
	lock(myself);
}


void Lock::lock(Applicant &myself)
{
	/*
	 * XXX: How to notice cancel-blocking signals issued when  being outside the
	 *      'l4_ipc_sleep' system call?
	 */
	while (!cmpxchg(&_state, UNLOCKED, LOCKED))
		Fiasco::l4_ipc_sleep(Fiasco::l4_ipc_timeout(0, 0, 500, 0));

	_owner = myself;
}


void Lock::unlock()
{
	_owner = Applicant(nullptr);
	memory_barrier();
	_state = UNLOCKED;
}
