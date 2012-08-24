/*
 * \brief  Copy thread state - x86_32
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
	_state.thread.ebp    = utcb->bp;
	_state.thread.eax    = utcb->ax;
	_state.thread.ebx    = utcb->bx;
	_state.thread.ecx    = utcb->cx;
	_state.thread.edx    = utcb->dx;
	_state.thread.esi    = utcb->si;
	_state.thread.edi    = utcb->di;

	_state.thread.gs     = utcb->gs.sel;
	_state.thread.fs     = utcb->fs.sel;
}
