/*
 * \brief   Kernel backend for virtual machines
 * \author  Stefan Kalkowski
 * \date    2015-02-10
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/thread.h>
#include <kernel/vm.h>

void Kernel::Thread::_call_new_vm()
{
	Signal_context * context =
		pd()->cap_tree().find<Signal_context>(user_arg_4());
	if (!context) {
		user_arg_0(cap_id_invalid());
		return;
	}

	_call_new<Vm>((Genode::Cpu_state_modes*)user_arg_2(), context,
	              (void*)user_arg_3());
}


void Kernel::Thread::_call_delete_vm() { _call_delete<Vm>(); }


void Kernel::Thread::_call_run_vm()
{
	reinterpret_cast<Vm*>(user_arg_1())->run();
	user_arg_0(0);
}


void Kernel::Thread::_call_pause_vm()
{
	reinterpret_cast<Vm*>(user_arg_1())->pause();
	user_arg_0(0);
}
