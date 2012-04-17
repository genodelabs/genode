/*
 * \brief  Fiasco.OC-specific supplements to the IPC framework
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2010-01-27
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__IPC_H_
#define _INCLUDE__BASE__IPC_H_

#include <base/ipc_generic.h>

namespace Fiasco {
#include <l4/sys/consts.h>
#include <l4/sys/task.h>
}

inline void Genode::Ipc_ostream::_marshal_capability(Genode::Native_capability const &cap)
{
	bool local = cap.local();
	long id    = local ? (long)cap.local() : cap.local_name();

	_write_to_buf(local);
	_write_to_buf(id);

	/* only transfer kernel-capability if it's no local capability and valid */
	if (!local && id)
		_snd_msg->snd_append_cap_sel(cap.dst());
}


inline void Genode::Ipc_istream::_unmarshal_capability(Genode::Native_capability &cap)
{
	using namespace Fiasco;

	bool local = false;
	long id    = 0;

	/* extract capability id from message buffer, and whether it's a local cap */
	_read_from_buf(local);
	_read_from_buf(id);

	/* if it's a local capability, the pointer is marshalled in the id */
	if (local) {
		cap = Capability<Native_capability>::local_cap((Native_capability*)id);
		return;
	}

	/* if id is zero an invalid capability was transfered */
	if (!id) {
		cap = Native_capability();
		return;
	}

	/* we received a valid, non-local capability, maybe we already own it? */
	Cap_index *i = cap_map()->find(id);
	Genode::addr_t rcv_cap = _rcv_msg->rcv_cap_sel();
	if (i) {
		/**
		 * If we've a dead capability in our database, which is already
		 * revoked, its id might be reused.
		 */
		l4_msgtag_t tag = l4_task_cap_valid(L4_BASE_TASK_CAP, i->kcap());
		if (!l4_msgtag_label(tag)) {
			i->inc();
			PWRN("leaking capability idx=%p id=%x ref_cnt=%d",i, i->id(), i->dec());
		} else {
			/* does somebody tries to fake us? */
			tag = l4_task_cap_equal(L4_BASE_TASK_CAP, i->kcap(), rcv_cap);
			if (!l4_msgtag_label(tag)) {
				PWRN("Got fake capability");
				cap = Native_capability();
			} else
				cap = Native_capability(i);
			return;
		}
	}
	/* insert the new capability in the map */
	i = cap_map()->insert(id);
	l4_task_map(L4_BASE_TASK_CAP, L4_BASE_TASK_CAP,
				l4_obj_fpage(rcv_cap, 0, L4_FPAGE_RWX),
				i->kcap() | L4_ITEM_MAP | L4_MAP_ITEM_GRANT);
	cap = Native_capability(i);
}

#endif /* _INCLUDE__BASE__IPC_H_ */
