/*
 * \brief   Kernel backend for virtual machines
 * \author  Stefan Kalkowski
 * \date    2015-02-10
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/thread.h>
#include <kernel/vm.h>

void Kernel::Thread::_call_delete_vm()
{
	Vm * const vm = Vm::pool()->object(user_arg_1());
	if (vm) vm->~Vm();
	user_arg_0(vm ? 0 : -1);
}


void Kernel::Thread::_call_run_vm() {
	Vm * const vm = Vm::pool()->object(user_arg_1());
	if (vm) vm->run();
	user_arg_0(vm ? 0 : -1);
}


void Kernel::Thread::_call_pause_vm()
{
	Vm * const vm = Vm::pool()->object(user_arg_1());
	if (vm) vm->pause();
	user_arg_0(vm ? 0 : -1);
}
