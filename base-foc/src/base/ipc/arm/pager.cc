/*
 * \brief  Fiasco.OC pager framework
 * \author Stefan Kalkowski
 * \date   2011-09-08
 *
 * ARM specific parts, when handling cpu-exceptions.
 */

/*
 * Copyright (C) 2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/ipc_pager.h>


void Genode::Ipc_pager::copy_regs(Thread_state *state)
{
	state->ip = _regs.pc;
	state->sp = _regs.sp;
	Genode::memcpy(&state->r, &_regs.r, sizeof(state->r));
	state->lr = _regs.ulr;
	state->cpsr = _regs.cpsr;
}
