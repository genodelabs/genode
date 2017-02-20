/*
 * \brief  Kernel-specific part of the CPU-session interface
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

#ifndef _CORE__INCLUDE__NATIVE_CPU_COMPONENT_H_
#define _CORE__INCLUDE__NATIVE_CPU_COMPONENT_H_

/* Genode includes */
#include <cpu_session/cpu_session.h>

/* core-local includes */
#include <rpc_cap_factory.h>

namespace Genode {

	class Cpu_session_component;
	class Native_cpu_component;
}


struct Genode::Native_cpu_component
{
	Native_cpu_component(Cpu_session_component &, char const *) { }

	Capability<Cpu_session::Native_cpu> cap()
	{
		return Capability<Cpu_session::Native_cpu>();
	}
};

#endif /* _CORE__INCLUDE__NATIVE_CPU_COMPONENT_H_ */
