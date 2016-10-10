/*
 * \brief   Kernel backend for virtual machines
 * \author  Stefan Kalkowski
 * \author  Reto Buerki
 * \author  Adrian-Ken Rueegsegger
 * \date    2015-06-03
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <platform_pd.h>
#include <kernel/vm.h>
#include <cpu/cpu_state.h>
#include <pic.h>

extern void * _vt_vm_entry;
extern void * _mt_client_context_ptr;

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


void Kernel::Vm::exception(unsigned const cpu_id)
{
	pause();
	if (_state->trapno == 200) {
		_context->submit(1);
		return;
	}

	if (_state->trapno >= Genode::Cpu_state::INTERRUPTS_START &&
		_state->trapno <= Genode::Cpu_state::INTERRUPTS_END) {
		pic()->irq_occurred(_state->trapno);
		_interrupt(cpu_id);
		_context->submit(1);
		return;
	}
	Genode::warning("VM: triggered unknown exception ", _state->trapno,
	                " with error code ", _state->errcode);
	assert(false);
}


void Kernel::Vm::proceed(unsigned const cpu_id)
{
	mtc()->switch_to(reinterpret_cast<Cpu::Context*>(_state), cpu_id,
	                 (addr_t) &_vt_vm_entry, (addr_t) &_mt_client_context_ptr);
}


void Kernel::Vm::inject_irq(unsigned irq) { }
