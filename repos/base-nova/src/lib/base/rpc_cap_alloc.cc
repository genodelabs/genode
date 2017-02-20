/*
 * \brief  Core-specific back end of the RPC entrypoint
 * \author Norman Feske
 * \date   2016-01-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include <util/retry.h>
#include <base/rpc_server.h>
#include <pd_session/client.h>

/* base-internal includes */
#include <base/internal/globals.h>

/* NOVA-specific part of the PD session interface */
#include <nova_native_pd/client.h>

using namespace Genode;


Native_capability Rpc_entrypoint::_alloc_rpc_cap(Pd_session &pd, Native_capability ep,
                                                 addr_t entry)
{
	if (!_native_pd_cap.valid())
		_native_pd_cap = pd.native_pd();

	Nova_native_pd_client native_pd(_native_pd_cap);

	Untyped_capability new_obj_cap =
		retry<Genode::Pd_session::Out_of_metadata>(
			[&] () {
				return native_pd.alloc_rpc_cap(ep, entry, 0);
			},
			[&] () {
				internal_env().upgrade(Parent::Env::pd(), "ram_quota=16K");
			});

	native_pd.imprint_rpc_cap(new_obj_cap, new_obj_cap.local_name());

	return new_obj_cap;
}


void Rpc_entrypoint::_free_rpc_cap(Pd_session &pd, Native_capability cap)
{
	return pd.free_rpc_cap(cap);
}
