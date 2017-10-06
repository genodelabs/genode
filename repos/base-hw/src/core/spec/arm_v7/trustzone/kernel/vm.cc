/*
 * \brief   Kernel backend for virtual machines
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2013-10-30
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/cpu.h>
#include <kernel/vm.h>

using namespace Kernel;


Kernel::Vm::Vm(void                   * const state,
               Kernel::Signal_context * const context,
               void                   * const table)
:  Cpu_job(Cpu_priority::MIN, 0),
  _state((Genode::Vm_state * const)state),
  _context(context), _table(0) {
	affinity(cpu_pool()->primary_cpu()); }


Kernel::Vm::~Vm() {}


void Vm::exception(Cpu & cpu)
{
	switch(_state->cpu_exception) {
	case Genode::Cpu_state::INTERRUPT_REQUEST:
	case Genode::Cpu_state::FAST_INTERRUPT_REQUEST:
		_interrupt(cpu.id());
		return;
	case Genode::Cpu_state::DATA_ABORT:
		_state->dfar = Cpu::Dfar::read();
	default:
		pause();
		_context->submit(1);
	}
}


bool secure_irq(unsigned const i);


extern "C" void monitor_mode_enter_normal_world(Cpu::Context*, void*);
extern void * kernel_stack;


void Vm::proceed(Cpu & cpu)
{
	unsigned const irq = _state->irq_injection;
	if (irq) {
		if (pic()->secure(irq)) {
			Genode::warning("Refuse to inject secure IRQ into VM");
		} else {
			pic()->trigger(irq);
			_state->irq_injection = 0;
		}
	}

	monitor_mode_enter_normal_world(reinterpret_cast<Cpu::Context*>(_state),
	                                (void*) cpu.stack_start());
}
