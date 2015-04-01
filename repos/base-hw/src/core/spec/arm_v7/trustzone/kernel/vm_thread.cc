/*
 * \brief   Kernel backend for thread-syscalls related to VMs
 * \author  Stefan Kalkowski
 * \date    2015-02-23
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/vm.h>
#include <kernel/thread.h>

void Kernel::Thread::_call_new_vm()
{
	/* lookup signal context */
	auto const context = Signal_context::pool()->object(user_arg_4());
	if (!context) {
		PWRN("failed to lookup signal context");
		user_arg_0(-1);
		return;
	}

	/* create virtual machine */
	typedef Genode::Cpu_state_modes Cpu_state_modes;
	void * const allocator = reinterpret_cast<void *>(user_arg_1());
	void * const table = reinterpret_cast<void *>(user_arg_3());
	Cpu_state_modes * const state =
		reinterpret_cast<Cpu_state_modes *>(user_arg_2());
	new (allocator) Vm(state, context, table);
	user_arg_0(0);
}
