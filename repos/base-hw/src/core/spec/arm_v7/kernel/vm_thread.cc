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
	reinterpret_cast<Vm*>(user_arg_1())->~Vm();
	user_arg_0(0);
}


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
