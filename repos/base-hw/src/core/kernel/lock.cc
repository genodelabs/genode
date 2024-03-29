/*
 * \brief  Kernel lock for multi-processor systems
 * \author Stefan Kalkowski
 * \date   2018-11-20
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <cpu/atomic.h>
#include <cpu/memory_barrier.h>

/* base-hw core includes */
#include <kernel/cpu.h>
#include <kernel/lock.h>


void Kernel::Lock::lock()
{
	/* check for the lock holder being the same cpu */
	if (_current_cpu == Cpu::executing_id()) {
		/* at least print an error message */
		Genode::raw("Cpu ", _current_cpu,
		            " error: re-entered lock. Kernel exception?!");
	}

	while (!Genode::cmpxchg((volatile int*)&_locked, UNLOCKED, LOCKED)) { ; }

	_current_cpu = Cpu::executing_id();
}


void Kernel::Lock::unlock()
{
	_current_cpu = INVALID;

	Genode::memory_barrier();
	_locked = UNLOCKED;
}
