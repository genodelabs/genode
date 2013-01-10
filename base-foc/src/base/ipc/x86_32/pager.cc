/*
 * \brief  Fiasco.OC pager framework
 * \author Stefan Kalkowski
 * \date   2011-09-08
 *
 * X86_32 specific parts, when handling cpu-exceptions.
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/ipc_pager.h>


void Genode::Ipc_pager::copy_regs(Thread_state *state)
{
	state->ip     = _regs.ip;
	state->sp     = _regs.sp;
	state->edi    = _regs.edi;
	state->esi    = _regs.esi;
	state->ebp    = _regs.ebp;
	state->ebx    = _regs.ebx;
	state->edx    = _regs.edx;
	state->ecx    = _regs.ecx;
	state->eax    = _regs.eax;
	state->gs     = _regs.gs;
	state->fs     = _regs.fs;
	state->eflags = _regs.flags;
	state->trapno = _regs.trapno;
}
