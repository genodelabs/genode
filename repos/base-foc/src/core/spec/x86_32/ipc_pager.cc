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

using namespace Core;


void Ipc_pager::get_regs(Foc_thread_state &state) const
{
	state.cpu.ip     = _regs.ip;
	state.cpu.sp     = _regs.sp;
	state.cpu.edi    = _regs.edi;
	state.cpu.esi    = _regs.esi;
	state.cpu.ebp    = _regs.ebp;
	state.cpu.ebx    = _regs.ebx;
	state.cpu.edx    = _regs.edx;
	state.cpu.ecx    = _regs.ecx;
	state.cpu.eax    = _regs.eax;
	state.cpu.gs     = _regs.gs;
	state.cpu.fs     = _regs.fs;
	state.cpu.eflags = _regs.flags;
	state.cpu.trapno = _regs.trapno;
}


void Ipc_pager::set_regs(Foc_thread_state const &state)
{
	_regs.ip     = state.cpu.ip;
	_regs.sp     = state.cpu.sp;
	_regs.edi    = state.cpu.edi;
	_regs.esi    = state.cpu.esi;
	_regs.ebp    = state.cpu.ebp;
	_regs.ebx    = state.cpu.ebx;
	_regs.edx    = state.cpu.edx;
	_regs.ecx    = state.cpu.ecx;
	_regs.eax    = state.cpu.eax;
	_regs.gs     = state.cpu.gs;
	_regs.fs     = state.cpu.fs;
	_regs.flags  = state.cpu.eflags;
	_regs.trapno = state.cpu.trapno;
}

