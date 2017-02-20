/*
 * \brief  Client-side I/O-memory session interface
 * \author Christian Helmuth
 * \date   2006-08-01
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__IO_MEM_SESSION__CLIENT_H_
#define _INCLUDE__IO_MEM_SESSION__CLIENT_H_

#include <io_mem_session/capability.h>
#include <base/rpc_client.h>

namespace Genode { struct Io_mem_session_client; }


struct Genode::Io_mem_session_client : Rpc_client<Io_mem_session>
{
	explicit Io_mem_session_client(Io_mem_session_capability session)
	: Rpc_client<Io_mem_session>(session) { }

	Io_mem_dataspace_capability dataspace() override { return call<Rpc_dataspace>(); }
};

#endif /* _INCLUDE__IO_MEM_SESSION__CLIENT_H_ */
