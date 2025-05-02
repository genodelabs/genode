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
#include <base/internal/globals.h>

using namespace Genode;


void Genode::init_rpc_cap_alloc(Parent &) { }


Rpc_entrypoint::Alloc_rpc_cap_result
Rpc_entrypoint::_alloc_rpc_cap(Pd_session &, Native_capability, addr_t)
{
	return with_native_thread(
		[&] (Native_thread &nt) { return nt.epoll.alloc_rpc_cap(); },
		[&]                     { return Native_capability(); });
}


void Rpc_entrypoint::_free_rpc_cap(Pd_session &, Native_capability cap)
{
	with_native_thread([&] (Native_thread &nt) {
		nt.epoll.free_rpc_cap(cap); });
}
