/*
 * \brief  Kernel mutex
 * \author Stefan Kalkowski
 * \date   2024-11-22
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <cpu/atomic.h>
#include <cpu/memory_barrier.h>

/* base-hw core includes */
#include <kernel/cpu.h>
#include <kernel/mutex.h>


bool Kernel::Mutex::_lock()
{
	while (!Genode::cmpxchg((volatile int*)&_locked, UNLOCKED, LOCKED))
		if (_current_cpu == Cpu::executing_id())
			return false;

	_current_cpu = Cpu::executing_id();
	return true;
}


void Kernel::Mutex::_unlock()
{
	_current_cpu = INVALID;

	Genode::memory_barrier();
	_locked = UNLOCKED;
}
