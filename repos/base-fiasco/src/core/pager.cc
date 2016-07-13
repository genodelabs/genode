/*
 * \brief  Pager support for Fiasco
 * \author Christian Helmuth
 * \date   2006-06-14
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>

/* core includes */
#include <ipc_pager.h>
#include <pager.h>

/* base-internal includes */
#include <base/internal/native_thread.h>
#include <base/internal/capability_space_tpl.h>

namespace Fiasco {
#include <l4/sys/ipc.h>
#include <l4/sys/syscalls.h>
}

using namespace Genode;
using namespace Fiasco;


/***************
 ** Ipc_pager **
 ***************/

void Ipc_pager::wait_for_fault()
{
	l4_msgdope_t  result;

	do {
		l4_ipc_wait(&_last,
		            L4_IPC_SHORT_MSG, &_pf_addr, &_pf_ip,
		            L4_IPC_NEVER, &result);

		if (L4_IPC_IS_ERROR(result))
			error(__func__, ": IPC error ", Hex(L4_IPC_ERROR(result)));

	} while (L4_IPC_IS_ERROR(result));
}


void Ipc_pager::reply_and_wait_for_fault()
{
	l4_msgdope_t  result;

	l4_ipc_reply_and_wait(_last,
	                      L4_IPC_SHORT_FPAGE, _reply_mapping.dst_addr(),
	                      _reply_mapping.fpage().fpage, &_last,
	                      L4_IPC_SHORT_MSG, &_pf_addr, &_pf_ip,
	                      L4_IPC_SEND_TIMEOUT_0, &result);

	if (L4_IPC_IS_ERROR(result)) {
		error(__func__, ": IPC error ", Hex(L4_IPC_ERROR(result)));

		/* ignore all errors and wait for next proper message */
		wait_for_fault();
	}
}


void Ipc_pager::acknowledge_wakeup()
{
	/* answer wakeup call from one of core's region-manager sessions */
	l4_msgdope_t result;
	l4_ipc_send(_last, L4_IPC_SHORT_MSG, 0, 0, L4_IPC_SEND_TIMEOUT_0, &result);
}


/**********************
 ** Pager Entrypoint **
 **********************/

Untyped_capability Pager_entrypoint::_pager_object_cap(unsigned long badge)
{
	return Capability_space::import(native_thread().l4id, Rpc_obj_key(badge));
}
