/*
 * \brief  RPC entrypoint support for allocating RPC object capabilities
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

using namespace Genode;


Native_capability Rpc_entrypoint::_alloc_rpc_cap(Pd_session &pd,
                                                 Native_capability, addr_t)
{
	for (;;) {

		Ram_quota ram_upgrade { 0 };
		Cap_quota cap_upgrade { 0 };

		try { return pd.alloc_rpc_cap(_cap); }
		catch (Out_of_ram)  { ram_upgrade = Ram_quota { 2*1024*sizeof(long) }; }
		catch (Out_of_caps) { cap_upgrade = Cap_quota { 4 }; }

		env_deprecated()->parent()->upgrade(Parent::Env::pd(),
		                                    String<100>("ram_quota=", ram_upgrade, ", "
		                                                "cap_quota=", cap_upgrade).string());
	}
}


void Rpc_entrypoint::_free_rpc_cap(Pd_session &pd, Native_capability cap)
{
	return pd.free_rpc_cap(cap);
}
