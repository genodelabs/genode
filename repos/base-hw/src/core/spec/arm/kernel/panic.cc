/*
 * \brief  Kernel panic
 * \author Stefan Kalkowski
 * \date   2019-04-05
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <kernel/cpu.h>
#include <kernel/kernel.h>


void Kernel::panic(Genode::Cpu_state * state) {

	Genode::raw("Kernel panic !!!");
	Genode::raw("Last saved cpu context follows:");
	Genode::raw("exc  : ", state->cpu_exception);
	Genode::raw("r0   : ", (void*)state->r0);
	Genode::raw("r1   : ", (void*)state->r1);
	Genode::raw("r2   : ", (void*)state->r2);
	Genode::raw("r3   : ", (void*)state->r3);
	Genode::raw("r4   : ", (void*)state->r4);
	Genode::raw("r5   : ", (void*)state->r5);
	Genode::raw("r6   : ", (void*)state->r6);
	Genode::raw("r7   : ", (void*)state->r7);
	Genode::raw("r8   : ", (void*)state->r8);
	Genode::raw("r9   : ", (void*)state->r9);
	Genode::raw("r10  : ", (void*)state->r10);
	Genode::raw("r11  : ", (void*)state->r11);
	Genode::raw("r12  : ", (void*)state->r12);
	Genode::raw("lr   : ", (void*)state->lr);
	Genode::raw("sp   : ", (void*)state->sp);
	Genode::raw("ip   : ", (void*)state->ip);
	Genode::raw("cpsr : ", (void*)state->cpsr);
	Genode::raw("dfar : ", Cpu::Dfar::read());
	Genode::raw("dfsr : ", Cpu::Dfsr::read());
	Genode::raw("ifar : ", Cpu::Ifar::read());
	Genode::raw("ifsr : ", Cpu::Ifsr::read());
	while (true) { ; }
}
