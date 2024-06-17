/*
 * \brief  Fiasco.OC pager framework
 * \author Stefan Kalkowski
 * \date   2011-09-08
 *
 * X86_64 specific parts, when handling cpu-exceptions.
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
	state.cpu.r8     = _regs.r8;
	state.cpu.r9     = _regs.r9;
	state.cpu.r10    = _regs.r10;
	state.cpu.r11    = _regs.r11;
	state.cpu.r12    = _regs.r12;
	state.cpu.r13    = _regs.r13;
	state.cpu.r14    = _regs.r14;
	state.cpu.r15    = _regs.r15;
	state.cpu.rax    = _regs.rax;
	state.cpu.rbx    = _regs.rbx;
	state.cpu.rcx    = _regs.rcx;
	state.cpu.rdx    = _regs.rdx;
	state.cpu.rdi    = _regs.rdi;
	state.cpu.rsi    = _regs.rsi;
	state.cpu.rbp    = _regs.rbp;
	state.cpu.ss     = _regs.ss;
	state.cpu.eflags = _regs.flags;
	state.cpu.trapno = _regs.trapno;
}


void Ipc_pager::set_regs(Foc_thread_state const &state)
{
	_regs.ip     = state.cpu.ip;
	_regs.sp     = state.cpu.sp;
	_regs.r8     = state.cpu.r8;
	_regs.r9     = state.cpu.r9;
	_regs.r10    = state.cpu.r10;
	_regs.r11    = state.cpu.r11;
	_regs.r12    = state.cpu.r12;
	_regs.r13    = state.cpu.r13;
	_regs.r14    = state.cpu.r14;
	_regs.r15    = state.cpu.r15;
	_regs.rax    = state.cpu.rax;
	_regs.rbx    = state.cpu.rbx;
	_regs.rcx    = state.cpu.rcx;
	_regs.rdx    = state.cpu.rdx;
	_regs.rdi    = state.cpu.rdi;
	_regs.rsi    = state.cpu.rsi;
	_regs.rbp    = state.cpu.rbp;
	_regs.ss     = state.cpu.ss;
	_regs.flags  = state.cpu.eflags;
	_regs.trapno = state.cpu.trapno;
}

