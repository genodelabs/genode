/*
 * \brief  RPC destination type
 * \author Norman Feske
 * \date   2016-03-11
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__INTERNAL__RPC_DESTINATION_H_
#define _INCLUDE__BASE__INTERNAL__RPC_DESTINATION_H_

#include <base/output.h>

namespace Genode {

	struct Rpc_destination
	{
		int socket = -1;

		explicit Rpc_destination(int socket) : socket(socket) { }

		Rpc_destination() { }
	};

	static inline Rpc_destination invalid_rpc_destination()
	{
		return Rpc_destination();
	}

	static void print(Output &out, Rpc_destination const &dst)
	{
		Genode::print(out, "socket=", dst.socket);
	}
}

#endif /* _INCLUDE__BASE__INTERNAL__RPC_DESTINATION_H_ */
