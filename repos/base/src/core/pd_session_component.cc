/*
 * \brief  Core implementation of the PD session interface
 * \author Christian Helmuth
 * \date   2006-07-17
 *
 * FIXME arg_string and quota missing
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode */
#include <base/printf.h>

/* Core */
#include <util.h>
#include <pd_session_component.h>
#include <cpu_session_component.h>

using namespace Genode;


int Pd_session_component::bind_thread(Thread_capability thread)
{
	Object_pool<Cpu_thread_component>::Guard cpu_thread(_thread_ep->lookup_and_lock(thread));
	if (!cpu_thread) return -1;

	if (cpu_thread->bound()) {
		PWRN("rebinding of threads not supported");
		return -2;
	}

	Platform_thread *p_thread = cpu_thread->platform_thread();

	int res = _pd.bind_thread(p_thread);

	if (res)
		return res;

	cpu_thread->bound(true);
	return 0;
}


int Pd_session_component::assign_parent(Parent_capability parent)
{
	return _pd.assign_parent(parent);
}
