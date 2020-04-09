/*
 * \brief  RPC destination type
 * \author Norman Feske
 * \date   2016-03-11
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__RPC_DESTINATION_H_
#define _INCLUDE__BASE__INTERNAL__RPC_DESTINATION_H_

#include <base/output.h>
#include <base/internal/rpc_obj_key.h>

#include <linux_syscalls.h>


namespace Genode { struct Rpc_destination; }


struct Genode::Rpc_destination
{
	Lx_sd socket;

	/*
	 * Distinction between a capability referring to a locally implemented
	 * RPC object and a capability referring to an RPC object hosted in
	 * a different component.
	 */
	bool foreign = true;

	Rpc_destination(Lx_sd socket) : socket(socket) { }

	bool valid() const { return socket.valid(); }

	static Rpc_destination invalid() { return Rpc_destination(Lx_sd::invalid()); }

	void print(Output &out) const
	{
		Genode::print(out, "socket=", socket, ",foreign=", foreign);
	}
};

#endif /* _INCLUDE__BASE__INTERNAL__RPC_DESTINATION_H_ */
