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

#include <kernel/cpu.h>
#include <kernel/lock.h>
#include <kernel/kernel.h>

Kernel::Lock & Kernel::data_lock()
{
	static Kernel::Lock lock;
	return lock;
}


void Kernel::Lock::lock()
{
	/* check for the lock holder being the same cpu */
	if (_current_cpu == Cpu::executing_id()) {
		/* at least print an error message */
		Genode::raw("Cpu ", _current_cpu,
		            " error: re-entered lock. Kernel exception?!");
		for (;;) ;
	}
	_lock.lock();
	_current_cpu = Cpu::executing_id();
}


void Kernel::Lock::unlock()
{
	_current_cpu = INVALID;
	_lock.unlock();
}
