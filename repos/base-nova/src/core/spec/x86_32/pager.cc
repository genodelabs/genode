/*
 * \brief  Copy thread state - x86_32
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
	_state.thread.eax       = utcb->ax;
	_state.thread.ecx       = utcb->cx;
	_state.thread.edx       = utcb->dx;
	_state.thread.ebx       = utcb->bx;

	_state.thread.ebp       = utcb->bp;
	_state.thread.esi       = utcb->si;
	_state.thread.edi       = utcb->di;

	_state.thread.sp        = utcb->sp;
	_state.thread.ip        = utcb->ip;
	_state.thread.eflags    = utcb->flags;

	_state.thread.exception = utcb->qual[0];
}

void Pager_object::_copy_state_to_utcb(Nova::Utcb * utcb)
{
	utcb->ax     = _state.thread.eax;
	utcb->cx     = _state.thread.ecx;
	utcb->dx     = _state.thread.edx;
	utcb->bx     = _state.thread.ebx;

	utcb->bp     = _state.thread.ebp;
	utcb->si     = _state.thread.esi;
	utcb->di     = _state.thread.edi;

	utcb->sp     = _state.thread.sp;
	utcb->ip     = _state.thread.ip;
	utcb->flags  = _state.thread.eflags;

	utcb->mtd = Nova::Mtd::ACDB |
	            Nova::Mtd::EBSD |
	            Nova::Mtd::ESP |
	            Nova::Mtd::EIP |
	            Nova::Mtd::EFL;
}
