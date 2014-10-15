/*
 * \brief  Pager support for seL4
 * \author Norman Feske
 * \date   2014-10-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/ipc_pager.h>
#include <base/printf.h>

using namespace Genode;


/***************
 ** IPC pager **
 ***************/

void Ipc_pager::wait_for_fault()
{
	PDBG("not implemented");
}


void Ipc_pager::reply_and_wait_for_fault()
{
	PDBG("not implemented");
}


void Ipc_pager::acknowledge_wakeup()
{
	PDBG("not implemented");
}


Ipc_pager::Ipc_pager() { }

