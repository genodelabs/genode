/*
 * \brief  Linux-specific supplements to the IPC framework
 * \author Norman Feske
 * \date   2012-07-26
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__IPC_H_
#define _INCLUDE__BASE__IPC_H_

#include <base/ipc_generic.h>


inline void Genode::Ipc_ostream::_marshal_capability(Genode::Native_capability const &cap)
{
	_write_to_buf(cap.local_name());

	if (cap.valid())
		_snd_msg->append_cap(cap.dst().socket);
}


inline void Genode::Ipc_istream::_unmarshal_capability(Genode::Native_capability &cap)
{
	long local_name =  0;
	_read_from_buf(local_name);

	int const socket = _rcv_msg->read_cap();
	cap = Native_capability(Cap_dst_policy::Dst(socket), local_name);
}

#endif /* _INCLUDE__BASE__IPC_H_ */
