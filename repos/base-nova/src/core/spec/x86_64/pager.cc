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

/* core includes */
#include <pager.h>

/* NOVA includes */
#include <nova/syscalls.h>

using namespace Core;

void Pager_object::_copy_state_from_utcb(Nova::Utcb const &utcb)
{
	_state.thread.cpu.rax       = utcb.ax;
	_state.thread.cpu.rcx       = utcb.cx;
	_state.thread.cpu.rdx       = utcb.dx;
	_state.thread.cpu.rbx       = utcb.bx;

	_state.thread.cpu.rbp       = utcb.bp;
	_state.thread.cpu.rsi       = utcb.si;
	_state.thread.cpu.rdi       = utcb.di;

	_state.thread.cpu.r8        = utcb.r8;
	_state.thread.cpu.r9        = utcb.r9;
	_state.thread.cpu.r10       = utcb.r10;
	_state.thread.cpu.r11       = utcb.r11;
	_state.thread.cpu.r12       = utcb.r12;
	_state.thread.cpu.r13       = utcb.r13;
	_state.thread.cpu.r14       = utcb.r14;
	_state.thread.cpu.r15       = utcb.r15;

	_state.thread.cpu.sp        = utcb.sp;
	_state.thread.cpu.ip        = utcb.ip;
	_state.thread.cpu.eflags    = utcb.flags;

	_state.thread.state = utcb.qual[0] ? Thread_state::State::EXCEPTION
	                                   : Thread_state::State::VALID;
}


void Pager_object::_copy_state_to_utcb(Nova::Utcb &utcb) const
{
	utcb.ax    = _state.thread.cpu.rax;
	utcb.cx    = _state.thread.cpu.rcx;
	utcb.dx    = _state.thread.cpu.rdx;
	utcb.bx    = _state.thread.cpu.rbx;

	utcb.bp    = _state.thread.cpu.rbp;
	utcb.si    = _state.thread.cpu.rsi;
	utcb.di    = _state.thread.cpu.rdi;

	utcb.r8    = _state.thread.cpu.r8;
	utcb.r9    = _state.thread.cpu.r9;
	utcb.r10   = _state.thread.cpu.r10;
	utcb.r11   = _state.thread.cpu.r11;
	utcb.r12   = _state.thread.cpu.r12;
	utcb.r13   = _state.thread.cpu.r13;
	utcb.r14   = _state.thread.cpu.r14;
	utcb.r15   = _state.thread.cpu.r15;

	utcb.sp    = _state.thread.cpu.sp;
	utcb.ip    = _state.thread.cpu.ip;
	utcb.flags = _state.thread.cpu.eflags;

	utcb.mtd   = Nova::Mtd::ACDB |
	             Nova::Mtd::EBSD |
	             Nova::Mtd::R8_R15 |
	             Nova::Mtd::EIP |
	             Nova::Mtd::ESP |
	             Nova::Mtd::EFL;
}
