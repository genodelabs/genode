/*
 * \brief  Copy thread state - x86_64
 * \author Alexander Boettcher
 * \date   2012-08-23
 */

/*
 * Copyright (C) 2012-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/pager.h>

/* NOVA includes */
#include <nova/syscalls.h>

using namespace Genode;

void Pager_object::_copy_state(Nova::Utcb * utcb)
{
	_state.thread.rbp    = utcb->bp;
	_state.thread.rax    = utcb->ax;
	_state.thread.rbx    = utcb->bx;
	_state.thread.rcx    = utcb->cx;
	_state.thread.rdx    = utcb->dx;
	_state.thread.rsi    = utcb->si;
	_state.thread.rdi    = utcb->di;

	_state.thread.r8     = utcb->r8;
	_state.thread.r9     = utcb->r9;
	_state.thread.r10    = utcb->r10;
	_state.thread.r11    = utcb->r11;
	_state.thread.r12    = utcb->r12;
	_state.thread.r13    = utcb->r13;
	_state.thread.r14    = utcb->r14;
	_state.thread.r15    = utcb->r15;

	_state.thread.ss     = utcb->ss.sel;
}
