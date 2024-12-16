/*
 * \brief  Pager support for Pistachio
 * \author Christian Helmuth
 * \date   2006-06-14
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base-internal includes */
#include <base/internal/native_thread.h>
#include <base/internal/capability_space_tpl.h>
#include <base/internal/pistachio.h>

/* core includes */
#include <ipc_pager.h>
#include <pager.h>

using namespace Core;
using namespace Pistachio;


/*************
 ** Mapping **
 *************/

/**
 * Prepare map operation
 *
 * On Pistachio, we need to map a page locally to be able to map it to another
 * address space.
 */
void Mapping::prepare_map_operation() const
{
	uint8_t * const core_local_addr = (uint8_t *)src_addr;

	if (writeable)
		touch_read_write(core_local_addr);
	else
		touch_read(core_local_addr);
}


/***************
 ** IPC pager **
 ***************/

void Ipc_pager::wait_for_fault()
{
	L4_MsgTag_t result;
	L4_ThreadId_t sender = L4_nilthread;
	bool failed;

	do {
		L4_Accept(L4_UntypedWordsAcceptor);
		result = L4_Wait(&sender);
		failed = L4_IpcFailed(result);
		if (failed)
			error("page fault IPC error (continuable)");

		if (L4_UntypedWords(result) != 2) {
			error("malformed page-fault ipc (sender=", sender, ")");
			failed = true;
		}

	} while (failed);

	L4_Msg_t msg;
	// TODO Error checking. Did we really receive 2 words?
	L4_Store(result, &msg);

	_pf_addr = L4_Get(&msg, 0);
	_pf_ip = L4_Get(&msg, 1);
	_flags = L4_Label(result);

	_last = sender;
}


void Ipc_pager::reply_and_wait_for_fault()
{
	/*
	 * XXX  call memory-control if mapping has enabled write-combining
	 */

	L4_Msg_t msg;
	L4_Accept(L4_UntypedWordsAcceptor);
	L4_Clear(&msg);

	/* this should work even if _map_item is a grant item */
	L4_Append(&msg, _map_item);
	L4_Load(&msg);
	L4_MsgTag_t result = L4_ReplyWait(_last, &_last);

	if (L4_IpcFailed(result)) {
		error("page fault IPC error (continuable)");
		wait_for_fault();
		return;
	}

	if (L4_UntypedWords(result) != 2) {
		error("malformed page-fault ipc. (sender=", _last, ")");
		wait_for_fault();
		return;
	}

	L4_Clear(&msg);
	// TODO Error checking. Did we really receive 2 words?
	L4_Store(result, &msg);

	_pf_addr = L4_Get(&msg, 0);
	_pf_ip = L4_Get(&msg, 1);
	_flags = L4_Label(result);
}


void Ipc_pager::acknowledge_wakeup()
{
	L4_Reply(_last);
}


/**********************
 ** Pager entrypoint **
 **********************/

Untyped_capability Pager_entrypoint::_pager_object_cap(unsigned long badge)
{
	return Capability_space::import(native_thread().l4id, Rpc_obj_key(badge));
}


void Core::init_page_fault_handling(Rpc_entrypoint &) { }
