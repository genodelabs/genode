/*
 * \brief  Fiasco.OC pager framework
 * \author Stefan Kalkowski
 * \date   2011-09-08
 *
 * X86_32 specific parts, when handling cpu-exceptions.
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <ipc_pager.h>


void Genode::Ipc_pager::get_regs(Foc_thread_state &state) const
{
	state.ip     = _regs.ip;
	state.sp     = _regs.sp;
	state.edi    = _regs.edi;
	state.esi    = _regs.esi;
	state.ebp    = _regs.ebp;
	state.ebx    = _regs.ebx;
	state.edx    = _regs.edx;
	state.ecx    = _regs.ecx;
	state.eax    = _regs.eax;
	state.gs     = _regs.gs;
	state.fs     = _regs.fs;
	state.eflags = _regs.flags;
	state.trapno = _regs.trapno;
}


void Genode::Ipc_pager::set_regs(Foc_thread_state const &state)
{
	_regs.ip     = state.ip;
	_regs.sp     = state.sp;
	_regs.edi    = state.edi;
	_regs.esi    = state.esi;
	_regs.ebp    = state.ebp;
	_regs.ebx    = state.ebx;
	_regs.edx    = state.edx;
	_regs.ecx    = state.ecx;
	_regs.eax    = state.eax;
	_regs.gs     = state.gs;
	_regs.fs     = state.fs;
	_regs.flags  = state.eflags;
	_regs.trapno = state.trapno;
}

