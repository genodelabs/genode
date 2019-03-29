/*
 * \brief  Fiasco.OC pager framework
 * \author Stefan Kalkowski
 * \date   2011-09-08
 *
 * ARM 64-bit specific parts, when handling cpu-exceptions.
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <ipc_pager.h>

namespace Fiasco {
#include <l4/sys/utcb.h>
}

enum Exceptions { EX_REGS = 0x500000 };


void Genode::Ipc_pager::_parse_exception()
{
	if (Fiasco::l4_utcb_exc()->err == EX_REGS)
		_type = PAUSE;
	else
		_type = EXCEPTION;
}


void Genode::Ipc_pager::get_regs(Foc_thread_state &state) const
{
	state.ip   = _regs.pc;
	state.sp   = _regs.sp;
}


void Genode::Ipc_pager::set_regs(Foc_thread_state const &state)
{
	_regs.pc    = state.ip;
	_regs.sp    = state.sp;
}

bool Genode::Ipc_pager::exec_fault() const
{
	return (_pf_addr & 4) && !(_pf_addr & 1);
}
