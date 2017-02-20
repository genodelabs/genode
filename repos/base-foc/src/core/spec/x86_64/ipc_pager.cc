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


void Genode::Ipc_pager::get_regs(Foc_thread_state *state)
{
	state->ip     = _regs.ip;
	state->sp     = _regs.sp;
	state->r8     = _regs.r8;
	state->r9     = _regs.r9;
	state->r10    = _regs.r10;
	state->r11    = _regs.r11;
	state->r12    = _regs.r12;
	state->r13    = _regs.r13;
	state->r14    = _regs.r14;
	state->r15    = _regs.r15;
	state->rax    = _regs.rax;
	state->rbx    = _regs.rbx;
	state->rcx    = _regs.rcx;
	state->rdx    = _regs.rdx;
	state->rdi    = _regs.rdi;
	state->rsi    = _regs.rsi;
	state->rbp    = _regs.rbp;
	state->ss     = _regs.ss;
	state->eflags = _regs.flags;
	state->trapno = _regs.trapno;
}


void Genode::Ipc_pager::set_regs(Foc_thread_state state)
{
	_regs.ip     = state.ip;
	_regs.sp     = state.sp;
	_regs.r8     = state.r8;
	_regs.r9     = state.r9;
	_regs.r10    = state.r10;
	_regs.r11    = state.r11;
	_regs.r12    = state.r12;
	_regs.r13    = state.r13;
	_regs.r14    = state.r14;
	_regs.r15    = state.r15;
	_regs.rax    = state.rax;
	_regs.rbx    = state.rbx;
	_regs.rcx    = state.rcx;
	_regs.rdx    = state.rdx;
	_regs.rdi    = state.rdi;
	_regs.rsi    = state.rsi;
	_regs.rbp    = state.rbp;
	_regs.ss     = state.ss;
	_regs.flags  = state.eflags;
	_regs.trapno = state.trapno;
}

