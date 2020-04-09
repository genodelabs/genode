/*
 * \brief  Core-specific back end of the RPC entrypoint
 * \author Norman Feske
 * \date   2020-04-07
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/rpc_server.h>

/* base-internal includes */
#include <base/internal/native_thread.h>

using namespace Genode;


Native_capability Rpc_entrypoint::_alloc_rpc_cap(Pd_session &, Native_capability,
                                                 addr_t)
{
	return Thread::native_thread().epoll.alloc_rpc_cap();
}


void Rpc_entrypoint::_free_rpc_cap(Pd_session &, Native_capability cap)
{
	Thread::native_thread().epoll.free_rpc_cap(cap);
}
