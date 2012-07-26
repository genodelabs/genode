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

#include <base/snprintf.h>

extern "C" int raw_write_str(const char *str);
extern "C" void wait_for_continue(void);

#define PRAWW(fmt, ...)                                             \
	do {                                                           \
		char str[128];                                             \
		Genode::snprintf(str, sizeof(str),                         \
		                 ESC_ERR fmt ESC_END "\n", ##__VA_ARGS__); \
		raw_write_str(str);                                        \
	} while (0)


inline void Genode::Ipc_ostream::_marshal_capability(Genode::Native_capability const &cap)
{

	PRAWW("_marshal_capability called: local_name=%ld, tid=%ld, socket=%d",
	 cap.local_name(), cap.dst().tid, cap.dst().socket);

	_write_to_buf(cap.local_name());
	_write_to_buf(cap.dst().tid);

	if (cap.valid()) {
		PRAWW("append_cap");
		_snd_msg->append_cap(cap.dst().socket);
	}
}


inline void Genode::Ipc_istream::_unmarshal_capability(Genode::Native_capability &cap)
{
	long local_name =  0;
	long tid        =  0;
	int  socket     = -1;

	_read_from_buf(local_name);
	_read_from_buf(tid);

	bool const cap_valid = (tid != 0);
	if (cap_valid)
		socket = _rcv_msg->read_cap();

	cap = Native_capability(Cap_dst_policy::Dst(tid, socket), local_name);
}

#endif /* _INCLUDE__BASE__IPC_H_ */
