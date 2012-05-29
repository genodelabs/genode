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
#include <util/assert.h>


inline void Genode::Ipc_ostream::_marshal_capability(Genode::Native_capability const &cap)
{
	using namespace Fiasco;

	/* first transfer local capability value */
	_write_to_buf(cap.local());

	/* if it's a local capability we're done */
	if (cap.local())
		return;

	if (cap.valid()) {
		if (!l4_msgtag_label(l4_task_cap_valid(L4_BASE_TASK_CAP, cap.dst()))) {
			_write_to_buf(0);
			return;
		}
	}

	/* transfer capability id */
	_write_to_buf(cap.local_name());

	/* only transfer kernel-capability if it's a valid one */
	if (cap.valid())
		_snd_msg->snd_append_cap_sel(cap.dst());

	ASSERT(!cap.valid() ||
	       l4_msgtag_label(l4_task_cap_valid(L4_BASE_TASK_CAP, cap.dst())),
	       "Send invalid cap");
}


inline void Genode::Ipc_istream::_unmarshal_capability(Genode::Native_capability &cap)
{
	using namespace Fiasco;

	long value = 0;

	/* get local capability pointer from message buffer */
	_read_from_buf(value);

	/* if it's a local capability, the pointer is marshalled in the id */
	if (value) {
		cap = Capability<Native_capability>::local_cap((Native_capability*)value);
		return;
	}

	/* extract capability id from message buffer */
	_read_from_buf(value);

	/* if id is zero an invalid capability was transfered */
	if (!value) {
		cap = Native_capability();
		return;
	}

	/* try to insert received capability in the map and return it */
	cap = Native_capability(cap_map()->insert_map(value, _rcv_msg->rcv_cap_sel()));
}

#endif /* _INCLUDE__BASE__IPC_H_ */
