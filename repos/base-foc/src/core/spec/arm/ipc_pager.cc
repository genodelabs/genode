/*
 * \brief  Fiasco.OC pager framework
 * \author Stefan Kalkowski
 * \date   2011-09-08
 *
 * ARM specific parts, when handling cpu-exceptions.
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <ipc_pager.h>

/* Fiasco.OC includes */
#include <foc/syscall.h>

using namespace Core;


enum Exceptions { EX_REGS = 0x500000 };


void Ipc_pager::_parse_exception()
{
	_type = (Foc::l4_utcb_exc()->err == EX_REGS) ? PAUSE : EXCEPTION;
}


void Ipc_pager::get_regs(Foc_thread_state &state) const
{
	state.ip   = _regs.pc;
	state.sp   = _regs.sp;
	state.r0   = _regs.r[0];
	state.r1   = _regs.r[1];
	state.r2   = _regs.r[2];
	state.r3   = _regs.r[3];
	state.r4   = _regs.r[4];
	state.r5   = _regs.r[5];
	state.r6   = _regs.r[6];
	state.r7   = _regs.r[7];
	state.r8   = _regs.r[8];
	state.r9   = _regs.r[9];
	state.r10  = _regs.r[10];
	state.r11  = _regs.r[11];
	state.r12  = _regs.r[12];
	state.lr   = _regs.ulr;
	state.cpsr = _regs.cpsr;
}


void Ipc_pager::set_regs(Foc_thread_state const &state)
{
	_regs.pc    = state.ip;
	_regs.sp    = state.sp;
	_regs.r[0]  = state.r0;
	_regs.r[1]  = state.r1;
	_regs.r[2]  = state.r2;
	_regs.r[3]  = state.r3;
	_regs.r[4]  = state.r4;
	_regs.r[5]  = state.r5;
	_regs.r[6]  = state.r6;
	_regs.r[7]  = state.r7;
	_regs.r[8]  = state.r8;
	_regs.r[9]  = state.r9;
	_regs.r[10] = state.r10;
	_regs.r[11] = state.r11;
	_regs.r[12] = state.r12;
	_regs.ulr   = state.lr;
	_regs.cpsr  = state.cpsr;
}


bool Ipc_pager::exec_fault() const
{
	return (_pf_addr & 4) && !(_pf_addr & 1);
}
