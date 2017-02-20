/*
 * \brief  Kernel-specific part of the PD-session interface
 * \author Norman Feske
 * \date   2016-01-19
 *
 * This definition is used on platforms with no kernel-specific PD functions
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__NATIVE_PD_COMPONENT_H_
#define _CORE__INCLUDE__NATIVE_PD_COMPONENT_H_

/* Genode includes */
#include <pd_session/pd_session.h>

/* core-local includes */
#include <rpc_cap_factory.h>

namespace Genode {

	class Pd_session_component;
	class Native_pd_component;
}


struct Genode::Native_pd_component
{
	Native_pd_component(Pd_session_component &, char const *) { }

	Capability<Pd_session::Native_pd> cap()
	{
		return Capability<Pd_session::Native_pd>();
	}
};

#endif /* _CORE__INCLUDE__NATIVE_PD_COMPONENT_H_ */
