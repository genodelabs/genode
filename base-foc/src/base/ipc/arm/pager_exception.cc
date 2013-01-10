/*
 * \brief  ARM-specific pager support for Fiasco.OC
 * \author Stefan Kalkowski
 * \date   2011-08-24
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/ipc_pager.h>

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
