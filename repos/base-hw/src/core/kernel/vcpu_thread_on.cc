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
#include <kernel/vcpu.h>
#include <kernel/cpu.h>

void Kernel::Thread::_call_new_vcpu()
{
	Signal_context * context =
		pd().cap_tree().find<Signal_context>((capid_t)user_arg_5());
	if (!context) {
		user_arg_0(cap_id_invalid());
		return;
	}

	_call_new<Vcpu>(_user_irq_pool, _cpu_pool.cpu((unsigned)user_arg_2()),
	              *(Board::Vcpu_data*)user_arg_3(),
	              *context, *(Vcpu::Identity*)user_arg_4());
}


void Kernel::Thread::_call_delete_vcpu()
{
	Core::Kernel_object<Vcpu> &to_delete =
		*(Core::Kernel_object<Vcpu>*)user_arg_1();

	/**
	 * Delete a vcpu immediately if it is assigned to this cpu,
	 * or the assigned cpu did not scheduled it.
	 */
	if (to_delete->_cpu().id() == Cpu::executing_id() ||
	    &to_delete->_cpu().current_context() != &*to_delete) {
		_call_delete<Vcpu>();
		return;
	}

	/**
	 * Construct a cross-cpu work item and send an IPI
	 */
	_vcpu_destroy.construct(*this, to_delete);
	to_delete->_cpu().trigger_ip_interrupt();
}


void Kernel::Thread::_call_run_vcpu()
{
	Object_identity_reference * ref = pd().cap_tree().find((capid_t)user_arg_1());
	Vcpu * vcpu = ref ? ref->object<Vcpu>() : nullptr;

	if (!vcpu) {
		Genode::raw("Invalid Vcpu cap");
		user_arg_0(-1);
		return;
	}

	vcpu->run();
	user_arg_0(0);
}


void Kernel::Thread::_call_pause_vcpu()
{
	Object_identity_reference * ref = pd().cap_tree().find((capid_t)user_arg_1());
	Vcpu * vcpu = ref ? ref->object<Vcpu>() : nullptr;

	if (!vcpu) {
		Genode::raw("Invalid Vcpu cap");
		user_arg_0(-1);
		return;
	}

	vcpu->pause();
	user_arg_0(0);
}
