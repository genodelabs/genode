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

namespace Fiasco {
#include <l4/sys/utcb.h>
}

enum Exceptions { EX_REGS = 0xff };


void Genode::Ipc_pager::_parse_exception()
{
	if (Fiasco::l4_utcb_exc()->trapno == EX_REGS)
		_type = PAUSE;
	else
		_type = EXCEPTION;
}
