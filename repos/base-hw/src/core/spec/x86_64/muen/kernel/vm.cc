/*
 * \brief   Kernel backend for virtual machines
 * \author  Stefan Kalkowski
 * \author  Reto Buerki
 * \author  Adrian-Ken Rueegsegger
 * \date    2015-06-03
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <assertion.h>
#include <platform_pd.h>
#include <kernel/cpu.h>
#include <kernel/vm.h>
#include <cpu/cpu_state.h>
#include <pic.h>

Kernel::Vm::Vm(void * const state, Kernel::Signal_context * const context,
               void * const)
: Cpu_job(Cpu_priority::MIN, 0),
  _state((Genode::Vm_state * const) state),
  _context(context),
  _table(nullptr)
{
	affinity(cpu_pool()->primary_cpu());
}


Kernel::Vm::~Vm() { }


void Kernel::Vm::exception(Cpu & cpu)
{
	pause();
	if (_state->trapno == 200) {
		_context->submit(1);
		return;
	}

	if (_state->trapno >= Genode::Cpu_state::INTERRUPTS_START &&
		_state->trapno <= Genode::Cpu_state::INTERRUPTS_END) {
		pic()->irq_occurred(_state->trapno);
		_interrupt(cpu.id());
		_context->submit(1);
		return;
	}
	Genode::warning("VM: triggered unknown exception ", _state->trapno,
	                " with error code ", _state->errcode);

	ASSERT_NEVER_CALLED;
}


void Kernel::Vm::proceed(Cpu & cpu)
{
	cpu.tss.ist[0] = (addr_t)_state + sizeof(Genode::Cpu_state);

	asm volatile("sti          \n"
	             "mov $1, %rax \n"
	             "vmcall");
}


void Kernel::Vm::inject_irq(unsigned) { }
