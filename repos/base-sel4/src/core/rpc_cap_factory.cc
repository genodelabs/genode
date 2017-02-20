/*
 * \brief  seL4-specific RPC capability factory
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
#include <base/capability.h>
#include <util/misc_math.h>

/* core includes */
#include <pd_session/pd_session.h>
#include <rpc_cap_factory.h>
#include <platform.h>

/* base-internal include */
#include <core_capability_space.h>

using namespace Genode;


static unsigned unique_id_cnt;


Native_capability Rpc_cap_factory::_alloc(Rpc_cap_factory *owner,
                                          Native_capability ep)
{
	if (!ep.valid()) {
		warning("Invalid entrypoint capability");
		return Native_capability();
	}

	Rpc_obj_key const rpc_obj_key(++unique_id_cnt);

	// XXX remove cast
	return Capability_space::create_rpc_obj_cap(ep, (Pd_session*)owner, rpc_obj_key);
}


Native_capability Rpc_cap_factory::alloc(Native_capability ep)
{
	return Rpc_cap_factory::_alloc(this, ep);
}


void Rpc_cap_factory::free(Native_capability cap)
{
	if (!cap.valid())
		return;

	/*
	 * XXX check whether this CAP session has created the capability to delete.
	 */

	static uint64_t leakage = 0;

	leakage ++;
	if (1ULL << log2(leakage) == leakage)
		warning(__PRETTY_FUNCTION__, " not implemented - resources leaked: ", Hex(leakage));
}

