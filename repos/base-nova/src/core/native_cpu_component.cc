/*
 * \brief  Core implementation of the CPU session interface extension
 * \author Norman Feske
 * \date   2016-04-21
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/stdint.h>

/* core-local includes */
#include <native_cpu_component.h>
#include <cpu_session_component.h>

using namespace Genode;


Native_capability
Native_cpu_component::pager_cap(Thread_capability thread_cap)
{
	auto lambda = [] (Cpu_thread_component *thread) {
		if (!thread)
			return Native_capability();

		return thread->platform_thread().pager()->cap();
	};
	return _thread_ep.apply(thread_cap, lambda);
}


Native_cpu_component::Native_cpu_component(Cpu_session_component &cpu_session, char const *)
:
	_cpu_session(cpu_session), _thread_ep(*_cpu_session._thread_ep)
{
	_thread_ep.manage(this);
}


Genode::Native_cpu_component::~Native_cpu_component()
{
	_thread_ep.dissolve(this);
}
