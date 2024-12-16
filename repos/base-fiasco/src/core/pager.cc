/*
 * \brief  Pager support for Fiasco
 * \author Christian Helmuth
 * \date   2006-06-14
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <ipc_pager.h>
#include <pager.h>

/* base-internal includes */
#include <base/internal/native_thread.h>
#include <base/internal/capability_space_tpl.h>

/* L4/Fiasco includes */
#include <fiasco/syscall.h>

using namespace Core;
using namespace Fiasco;


/**
 * Prepare map operation
 *
 * On Fiasco, we need to map a page locally to be able to map it to another
 * address space.
 */
void Mapping::prepare_map_operation() const
{
	addr_t const core_local_addr = src_addr;
	size_t const mapping_size    = 1UL << size_log2;

	for (addr_t i = 0; i < mapping_size; i += L4_PAGESIZE) {
		if (writeable)
			touch_read_write((unsigned char volatile *)(core_local_addr + i));
		else
			touch_read((unsigned char const volatile *)(core_local_addr + i));
	}
}


/***************
 ** Ipc_pager **
 ***************/

void Ipc_pager::wait_for_fault()
{
	l4_msgdope_t result;

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
	l4_fpage_t const fpage { l4_fpage(_reply_mapping.src_addr,
	                                  _reply_mapping.size_log2,
	                                  _reply_mapping.writeable, false) };

	l4_msgdope_t result;
	l4_ipc_reply_and_wait(_last, L4_IPC_SHORT_FPAGE,
	                      _reply_mapping.dst_addr, fpage.fpage, &_last,
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


void Core::init_page_fault_handling(Rpc_entrypoint &) { }
