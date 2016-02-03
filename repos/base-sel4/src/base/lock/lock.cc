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

/* seL4 includes */
#include <sel4/sel4.h>

using namespace Genode;


Cancelable_lock::Cancelable_lock(Cancelable_lock::State initial)
: _lock(UNLOCKED)
{
	if (initial == LOCKED)
		lock();
}


void Cancelable_lock::lock()
{
	while (!Genode::cmpxchg(&_lock, UNLOCKED, LOCKED))
		seL4_Yield();
}


void Cancelable_lock::unlock()
{
	Genode::memory_barrier();
	_lock = UNLOCKED;
}
