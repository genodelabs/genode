/*
 * \brief  Kernel mutex
 * \author Stefan Kalkowski
 * \date   2018-11-20
 */

/*
 * Copyright (C) 2019-2024 Genode Labs GmbH
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
	if (_current_cpu == Cpu::executing_id())
		return false;

	Cpu::wait_for_xchg((volatile int*)&_locked, LOCKED, UNLOCKED);
	_current_cpu = Cpu::executing_id();
	return true;
}


void Kernel::Mutex::_unlock()
{
	_current_cpu = INVALID;

	Genode::memory_barrier();
	_locked = UNLOCKED;
	Cpu::wakeup_waiting_cpus();
}
