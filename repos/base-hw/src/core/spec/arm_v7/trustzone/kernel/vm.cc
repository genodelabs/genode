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
#include <kernel/vm.h>

extern void *         _mt_nonsecure_entry_pic;
extern Genode::addr_t _tz_client_context;
extern Genode::addr_t _mt_master_context_begin;
extern Genode::addr_t _tz_master_context;

using namespace Kernel;


Kernel::Vm::Vm(void                   * const state,
               Kernel::Signal_context * const context,
               void                   * const table)
:  Cpu_job(Cpu_priority::MIN, 0),
  _state((Genode::Vm_state * const)state),
  _context(context), _table(0)
{
	affinity(cpu_pool()->primary_cpu());

	Genode::memcpy(&_tz_master_context, &_mt_master_context_begin,
	               sizeof(Cpu_context));
}


Kernel::Vm::~Vm() {}


void Vm::exception(unsigned const cpu)
{
	switch(_state->cpu_exception) {
	case Genode::Cpu_state::INTERRUPT_REQUEST:
	case Genode::Cpu_state::FAST_INTERRUPT_REQUEST:
		_interrupt(cpu);
		return;
	case Genode::Cpu_state::DATA_ABORT:
		_state->dfar = Cpu::Dfar::read();
	default:
		pause();
		_context->submit(1);
	}
}


bool secure_irq(unsigned const i);

void Vm::proceed(unsigned const cpu)
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
	mtc()->switch_to(reinterpret_cast<Cpu::Context*>(_state), cpu,
	                 (addr_t)&_mt_nonsecure_entry_pic,
	                 (addr_t)&_tz_client_context);
}
