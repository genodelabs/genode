/*
 * \brief  NOVA-specific supplements to the IPC framework
 * \author Norman Feske
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


inline void Genode::Ipc_ostream::_marshal_capability(Genode::Native_capability const &cap)
{
	long unique_id = cap.local_name();
	_write_to_buf(unique_id);
	_snd_msg->snd_append_pt_sel(cap.tid());
}


inline void Genode::Ipc_istream::_unmarshal_capability(Genode::Native_capability &cap)
{
	long unique_id = 0;
	_read_from_buf(unique_id);
	int pt_sel = _rcv_msg->rcv_pt_sel();
	cap = Native_capability(pt_sel, unique_id);
}

#endif /* _INCLUDE__BASE__IPC_H_ */
