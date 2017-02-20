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

#include <pd_session_component.h>
#include <native_pd_component.h>

using namespace Genode;


Native_capability Native_pd_component::task_cap()
{
	return Native_capability(_pd_session._pd.native_task());
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


