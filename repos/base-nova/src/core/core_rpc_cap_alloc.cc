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
#include <base/rpc_server.h>

/* core includes */
#include <platform_generic.h>
#include <rpc_cap_factory.h>
#include <imprint_badge.h>

/* base-internal includes */
#include <base/internal/globals.h>

using namespace Genode;


void Genode::init_rpc_cap_alloc(Parent &) { }


static Core::Rpc_cap_factory &rpc_cap_factory()
{
	static Core::Rpc_cap_factory inst(Core::platform().core_mem_alloc());
	return inst;
}


Rpc_entrypoint::Alloc_rpc_cap_result
Rpc_entrypoint::_alloc_rpc_cap(Pd_session &, Native_capability ep, addr_t entry)
{
	return rpc_cap_factory().alloc(ep, entry, 0);
}


void Rpc_entrypoint::_free_rpc_cap(Pd_session &, Native_capability cap)
{
	rpc_cap_factory().free(cap);
}
