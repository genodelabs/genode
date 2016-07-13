/*
 * \brief   Kernel back-end for execution contexts in userland
 * \author  Martin Stein
 * \author  Reto Buerki
 * \author  Stefan Kalkowski
 * \date    2013-11-11
 */

/*
 * Copyright (C) 2013-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/thread.h>
#include <kernel/pd.h>

void Kernel::Thread::_mmu_exception()
{
	_become_inactive(AWAITS_RESUME);
	_fault_pd     = (addr_t)_pd->platform_pd();
	_fault_signal = (addr_t)_fault.signal_context();
	_fault_addr   = Cpu::Cr2::read();

	/*
	 * Core should never raise a page-fault. If this happens, print out an
	 * error message with debug information.
	 */
	if (_pd == Kernel::core_pd())
		Genode::error("page fault in core thread (", label(), "): "
		              "ip=", Genode::Hex(ip), " fault=", Genode::Hex(_fault_addr));

	_fault.submit();
	return;
}


void Kernel::Thread::_init() { }


void Kernel::Thread::_call_update_pd() { }
