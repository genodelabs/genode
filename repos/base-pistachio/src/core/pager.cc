/*
 * \brief  Pager support for Pistachio
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
#include <base/sleep.h>

/* base-internal includes */
#include <base/internal/native_thread.h>
#include <base/internal/capability_space_tpl.h>

/* core includes */
#include <ipc_pager.h>
#include <pager.h>

namespace Pistachio
{
#include <l4/message.h>
#include <l4/ipc.h>
#include <l4/schedule.h>
#include <l4/kdebug.h>
}

using namespace Genode;
using namespace Pistachio;


/*************
 ** Mapping **
 *************/

Mapping::Mapping(addr_t dst_addr, addr_t src_addr,
                 Cache_attribute, bool io_mem, unsigned l2size,
                 bool rw, bool grant)
{
	L4_Fpage_t fpage = L4_FpageLog2(src_addr, l2size);

	fpage += rw ? L4_FullyAccessible : L4_Readable;

	if (grant)
		_grant_item = L4_GrantItem(fpage, dst_addr);
	else
		_map_item = L4_MapItem(fpage, dst_addr);
}


Mapping::Mapping() { _map_item = L4_MapItem(L4_Nilpage, 0); }


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
