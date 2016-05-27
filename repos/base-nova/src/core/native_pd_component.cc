/*
 * \brief  Kernel-specific part of the PD-session interface
 * \author Norman Feske
 * \date   2016-01-19
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <pd_session_component.h>
#include <native_pd_component.h>

/* core-local includes */
#include <imprint_badge.h>

using namespace Genode;


Native_capability Native_pd_component::alloc_rpc_cap(Native_capability ep,
                                                     addr_t entry, addr_t mtd)
{
	try {
		return _pd_session._rpc_cap_factory.alloc(ep, entry, mtd); }

	catch (Allocator::Out_of_memory) {
		throw Pd_session::Out_of_metadata(); }
}


void Native_pd_component::imprint_rpc_cap(Native_capability cap, unsigned long badge)
{
	if (cap.valid())
		imprint_badge(cap.local_name(), badge);
}


Native_pd_component::Native_pd_component(Pd_session_component &pd_session,
                                         char const *args)
:
	_pd_session(pd_session)
{
	_pd_session._thread_ep.manage(this);
}


Native_pd_component::~Native_pd_component()
{
	_pd_session._thread_ep.dissolve(this);
}


