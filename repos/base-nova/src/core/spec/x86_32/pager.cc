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

/* core includes */
#include <pager.h>

/* NOVA includes */
#include <nova/syscalls.h>

using namespace Core;

void Pager_object::_copy_state_from_utcb(Nova::Utcb const &utcb)
{
	_state.thread.cpu.eax    = utcb.ax;
	_state.thread.cpu.ecx    = utcb.cx;
	_state.thread.cpu.edx    = utcb.dx;
	_state.thread.cpu.ebx    = utcb.bx;

	_state.thread.cpu.ebp    = utcb.bp;
	_state.thread.cpu.esi    = utcb.si;
	_state.thread.cpu.edi    = utcb.di;

	_state.thread.cpu.sp     = utcb.sp;
	_state.thread.cpu.ip     = utcb.ip;
	_state.thread.cpu.eflags = utcb.flags;

	_state.thread.state = utcb.qual[0] ? Thread_state::State::EXCEPTION
	                                   : Thread_state::State::VALID;
}


void Pager_object::_copy_state_to_utcb(Nova::Utcb &utcb) const
{
	utcb.ax    = _state.thread.cpu.eax;
	utcb.cx    = _state.thread.cpu.ecx;
	utcb.dx    = _state.thread.cpu.edx;
	utcb.bx    = _state.thread.cpu.ebx;

	utcb.bp    = _state.thread.cpu.ebp;
	utcb.si    = _state.thread.cpu.esi;
	utcb.di    = _state.thread.cpu.edi;

	utcb.sp    = _state.thread.cpu.sp;
	utcb.ip    = _state.thread.cpu.ip;
	utcb.flags = _state.thread.cpu.eflags;

	utcb.mtd   = Nova::Mtd::ACDB |
	             Nova::Mtd::EBSD |
	             Nova::Mtd::ESP  |
	             Nova::Mtd::EIP  |
	             Nova::Mtd::EFL;
}
