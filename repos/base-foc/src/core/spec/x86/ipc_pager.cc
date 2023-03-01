/*
 * \brief  x86-specific pager support for Fiasco.OC
 * \author Stefan Kalkowski
 * \date   2011-08-24
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


enum Exceptions { EX_REGS = 0xff };


void Ipc_pager::_parse_exception()
{
	_type = (Foc::l4_utcb_exc()->trapno == EX_REGS) ? PAUSE : EXCEPTION;
}


bool Ipc_pager::exec_fault() const
{
	return ((_pf_addr & 1) && !write_fault());
}
