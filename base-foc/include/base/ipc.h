/*
 * \brief  Fiasco.OC-specific supplements to the IPC framework
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2010-01-27
 */

/*
 * Copyright (C) 2010-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__IPC_H_
#define _INCLUDE__BASE__IPC_H_

#include <base/ipc_generic.h>

namespace Fiasco {
#include <l4/sys/consts.h>
#include <l4/sys/kdebug.h>
#include <l4/sys/task.h>
}

inline void Genode::Ipc_ostream::_marshal_capability(Genode::Native_capability const &cap)
{
	long unique_id = cap.local_name();
	_write_to_buf(unique_id);
	if (unique_id)
		_snd_msg->snd_append_cap_sel(cap.dst());
}


inline void Genode::Ipc_istream::_unmarshal_capability(Genode::Native_capability &cap)
{
	using namespace Fiasco;

	/* extract Genode internal capability label from message buffer */
	long unique_id = 0;
	_read_from_buf(unique_id);

	if (!unique_id) {
		cap = Native_capability();
		return;
	}

	/* allocate new cap slot and grant cap to it out of receive window */
	Genode::addr_t cap_sel = Capability_allocator::allocator()->alloc();
	l4_task_map(L4_BASE_TASK_CAP, L4_BASE_TASK_CAP,
	            l4_obj_fpage(_rcv_msg->rcv_cap_sel(), 0, L4_FPAGE_RWX),
	            cap_sel | L4_ITEM_MAP | L4_MAP_ITEM_GRANT);
	cap = Native_capability(cap_sel, unique_id);
}

#endif /* _INCLUDE__BASE__IPC_H_ */
