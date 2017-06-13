/*
 * \brief  Kernel-specific part of the PD-session interface
 * \author Stefan Kalkowski
 * \date   2017-06-13
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <pd_session_component.h>
#include <native_pd_component.h>

using namespace Genode;

void Native_pd_component::upgrade_cap_slab() {
	_pd_session._pd->upgrade_slab(_pd_session._sliced_heap);
	//throw Out_of_ram();
}


Native_pd_component::Native_pd_component(Pd_session_component &pd_session,
                                         char const *args)
: _pd_session(pd_session) {
	_pd_session._ep.manage(this); }


Native_pd_component::~Native_pd_component() {
	_pd_session._ep.dissolve(this); }
