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
#include <cpu/vm_state_trustzone.h>

using namespace Kernel;


Vm::Vm(Irq::Pool              & user_irq_pool,
       Cpu                    & cpu,
       Genode::Vm_state       & state,
       Kernel::Signal_context & context,
       Identity               & id)
:
	Kernel::Object { *this },
	Cpu_job(Cpu_priority::min(), 0),
	_user_irq_pool(user_irq_pool),
	_state(state),
	_context(context),
	_id(id),
	_vcpu_context(cpu)
{
	affinity(cpu);
}


Vm::~Vm() {}


void Vm::exception(Cpu & cpu)
{
	switch(_state.cpu_exception) {
	case Genode::Cpu_state::INTERRUPT_REQUEST: [[fallthrough]];
	case Genode::Cpu_state::FAST_INTERRUPT_REQUEST:
		_interrupt(_user_irq_pool, cpu.id());
		return;
	case Genode::Cpu_state::DATA_ABORT:
		_state.dfar = Cpu::Dfar::read();
		[[fallthrough]];
	default:
		pause();
		_context.submit(1);
	}
}


bool secure_irq(unsigned const i);


extern "C" void monitor_mode_enter_normal_world(Genode::Vm_state&, void*);
extern void * kernel_stack;


void Vm::proceed(Cpu & cpu)
{
	unsigned const irq = _state.irq_injection;
	if (irq) {
		if (cpu.pic().secure(irq)) {
			Genode::raw("Refuse to inject secure IRQ into VM");
		} else {
			cpu.pic().trigger(irq);
			_state.irq_injection = 0;
		}
	}

	monitor_mode_enter_normal_world(_state, (void*) cpu.stack_start());
}
