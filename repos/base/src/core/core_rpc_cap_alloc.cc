/*
 * \brief  Core-specific back end of the RPC entrypoint
 * \author Norman Feske
 * \date   2016-01-19
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/rpc_server.h>

/* core-local includes */
#include <platform_generic.h>
#include <rpc_cap_factory.h>

using namespace Genode;


static Rpc_cap_factory &rpc_cap_factory()
{
	static Rpc_cap_factory inst(*platform()->core_mem_alloc());
	return inst;
}


Native_capability Rpc_entrypoint::_alloc_rpc_cap(Pd_session &, Native_capability ep,
                                                 addr_t)
{
	return rpc_cap_factory().alloc(ep);
}


void Rpc_entrypoint::_free_rpc_cap(Pd_session &, Native_capability cap)
{
	rpc_cap_factory().free(cap);
}
