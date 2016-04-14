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


void Pd_session_component::bind_thread(Thread_capability thread)
{
	return _thread_ep.apply(thread, [&] (Cpu_thread_component *cpu_thread) {
		if (!cpu_thread) return;

		if (cpu_thread->bound()) {
			PWRN("rebinding of threads not supported");
			return;
		}

		Platform_thread *p_thread = cpu_thread->platform_thread();

		_pd.bind_thread(p_thread);

		cpu_thread->bound(true);
	});
}


void Pd_session_component::assign_parent(Parent_capability parent)
{
	_pd.assign_parent(parent);
}
