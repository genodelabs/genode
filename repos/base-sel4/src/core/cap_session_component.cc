/*
 * \brief  seL4-specific capability allocation
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2015-05-08
 *
 * Based on base-foc/src/core/cap_session_component.cc
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/capability.h>
#include <util/misc_math.h>

/* core includes */
#include <cap_session_component.h>
#include <platform.h>

/* base-internal include */
#include <core_capability_space.h>

using namespace Genode;


static unsigned unique_id_cnt;


Native_capability Cap_session_component::alloc(Cap_session_component *session,
                                               Native_capability ep)
{
	if (!ep.valid()) {
		PWRN("Invalid entrypoint capability");
		return Native_capability();
	}

	Rpc_obj_key const rpc_obj_key(++unique_id_cnt);

	return Capability_space::create_rpc_obj_cap(ep, session, rpc_obj_key);
}


Native_capability Cap_session_component::alloc(Native_capability ep)
{
	return Cap_session_component::alloc(this, ep);
}


void Cap_session_component::free(Native_capability cap)
{
	if (!cap.valid())
		return;

	PDBG("not implemented");

	/*
	 * XXX check whether this CAP session has created the capability to delete.
	 */
}

