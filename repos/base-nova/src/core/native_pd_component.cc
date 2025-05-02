/*
 * \brief  Kernel-specific part of the PD-session interface
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
#include <pd_session_component.h>
#include <native_pd_component.h>
#include <imprint_badge.h>

using namespace Core;


Pd_session::Alloc_rpc_cap_result
Native_pd_component::alloc_rpc_cap(Native_capability ep, addr_t entry, addr_t mtd)
{
	_pd_session._consume_cap(Pd_session_component::RPC_CAP);

	Alloc_rpc_cap_result result = _pd_session._rpc_cap_factory.alloc(ep, entry, mtd);

	if (result.failed())
		_pd_session._released_cap_silent();

	return result;
}


void Native_pd_component::imprint_rpc_cap(Native_capability cap, unsigned long badge)
{
	if (cap.valid())
		imprint_badge(cap.local_name(), badge);
}


Native_pd_component::Native_pd_component(Pd_session_component &pd, char const *)
:
	_pd_session(pd)
{
	_pd_session._ep.manage(this);
}


Native_pd_component::~Native_pd_component()
{
	_pd_session._ep.dissolve(this);
}


