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
#include <kernel/cpu.h>

void Kernel::Thread::_call_new_vm()
{
	Signal_context * context =
		pd().cap_tree().find<Signal_context>((capid_t)user_arg_5());
	if (!context) {
		user_arg_0(cap_id_invalid());
		return;
	}

	_call_new<Vm>(_user_irq_pool, _cpu_pool.cpu((unsigned)user_arg_2()),
	              *(Board::Vcpu_data*)user_arg_3(),
	              *context, *(Vm::Identity*)user_arg_4());
}


void Kernel::Thread::_call_delete_vm() { _call_delete<Vm>(); }


void Kernel::Thread::_call_run_vm()
{
	Object_identity_reference * ref = pd().cap_tree().find((capid_t)user_arg_1());
	Vm * vm = ref ? ref->object<Vm>() : nullptr;

	if (!vm) {
		Genode::raw("Invalid VM cap");
		user_arg_0(-1);
		return;
	}

	vm->run();
	user_arg_0(0);
}


void Kernel::Thread::_call_pause_vm()
{
	Object_identity_reference * ref = pd().cap_tree().find((capid_t)user_arg_1());
	Vm * vm = ref ? ref->object<Vm>() : nullptr;

	if (!vm) {
		Genode::raw("Invalid VM cap");
		user_arg_0(-1);
		return;
	}

	vm->pause();
	user_arg_0(0);
}
