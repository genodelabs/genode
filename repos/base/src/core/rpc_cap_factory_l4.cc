/*
 * \brief  RPC capability factory
 * \author Norman Feske
 * \date   2016-01-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <rpc_cap_factory.h>
#include <platform.h>

/* base-internal include */
#include <core_capability_space.h>
#include <base/internal/capability_space_tpl.h>

using namespace Core;


static unsigned unique_id_cnt;


Rpc_cap_factory::Alloc_result
Rpc_cap_factory::_alloc(Rpc_cap_factory &, Native_capability ep)
{
	if (!ep.valid()) {
		warning("Invalid entrypoint capability");
		return Alloc_error::DENIED;
	}

	Rpc_obj_key const rpc_obj_key(++unique_id_cnt);

	/* combine thread ID of 'ep' with new unique ID */
	Genode::Capability_space::Ipc_cap_data cap_data =
		Genode::Capability_space::ipc_cap_data(ep);

	return Genode::Capability_space::import(cap_data.dst, rpc_obj_key);
}


Rpc_cap_factory::Alloc_result Rpc_cap_factory::alloc(Native_capability ep)
{
	return Rpc_cap_factory::_alloc(*this, ep);
}


void Rpc_cap_factory::free(Native_capability) { }

