/*
 * \brief   Kernel backend for virtual machines
 * \author  Martin Stein
 * \date    2013-10-30
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/vm.h>

using namespace Kernel;


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
		Cpu_job::_deactivate_own_share();
		_context->submit(1);
	}
}
