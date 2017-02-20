/*
 * \brief  Copy thread state - x86_64
 * \author Alexander Boettcher
 * \date   2012-08-23
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Core includes */
#include <pager.h>

/* NOVA includes */
#include <nova/syscalls.h>

using namespace Genode;

void Pager_object::_copy_state_from_utcb(Nova::Utcb * utcb)
{
	_state.thread.rax       = utcb->ax;
	_state.thread.rcx       = utcb->cx;
	_state.thread.rdx       = utcb->dx;
	_state.thread.rbx       = utcb->bx;

	_state.thread.rbp       = utcb->bp;
	_state.thread.rsi       = utcb->si;
	_state.thread.rdi       = utcb->di;

	_state.thread.r8        = utcb->r8;
	_state.thread.r9        = utcb->r9;
	_state.thread.r10       = utcb->r10;
	_state.thread.r11       = utcb->r11;
	_state.thread.r12       = utcb->r12;
	_state.thread.r13       = utcb->r13;
	_state.thread.r14       = utcb->r14;
	_state.thread.r15       = utcb->r15;

	_state.thread.sp        = utcb->sp;
	_state.thread.ip        = utcb->ip;
	_state.thread.eflags    = utcb->flags;

	_state.thread.exception = utcb->qual[0];
}


void Pager_object::_copy_state_to_utcb(Nova::Utcb * utcb)
{
	utcb->ax     = _state.thread.rax;
	utcb->cx     = _state.thread.rcx;
	utcb->dx     = _state.thread.rdx;
	utcb->bx     = _state.thread.rbx;

	utcb->bp     = _state.thread.rbp;
	utcb->si     = _state.thread.rsi;
	utcb->di     = _state.thread.rdi;

	utcb->r8     = _state.thread.r8;
	utcb->r9     = _state.thread.r9;
	utcb->r10    = _state.thread.r10;
	utcb->r11    = _state.thread.r11;
	utcb->r12    = _state.thread.r12;
	utcb->r13    = _state.thread.r13;
	utcb->r14    = _state.thread.r14;
	utcb->r15    = _state.thread.r15;

	utcb->sp     = _state.thread.sp;
	utcb->ip     = _state.thread.ip;
	utcb->flags  = _state.thread.eflags;

	utcb->mtd = Nova::Mtd::ACDB |
	            Nova::Mtd::EBSD |
	            Nova::Mtd::R8_R15 |
	            Nova::Mtd::EIP |
	            Nova::Mtd::ESP |
	            Nova::Mtd::EFL;
}
