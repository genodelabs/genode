/*
 * \brief  seL4-specific RPC capability factory
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
#include <base/capability.h>
#include <util/misc_math.h>

/* core includes */
#include <cap_session/cap_session.h>
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
		PWRN("Invalid entrypoint capability");
		return Native_capability();
	}

	Rpc_obj_key const rpc_obj_key(++unique_id_cnt);

	// XXX remove cast
	return Capability_space::create_rpc_obj_cap(ep, (Cap_session*)owner, rpc_obj_key);
}


Native_capability Rpc_cap_factory::alloc(Native_capability ep)
{
	return Rpc_cap_factory::_alloc(this, ep);
}


void Rpc_cap_factory::free(Native_capability cap)
{
	if (!cap.valid())
		return;

	PDBG("not implemented");

	/*
	 * XXX check whether this CAP session has created the capability to delete.
	 */
}

