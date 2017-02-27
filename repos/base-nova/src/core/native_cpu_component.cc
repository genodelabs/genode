/*
 * \brief  Core implementation of the CPU session interface extension
 * \author Norman Feske
 * \date   2016-04-21
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/stdint.h>

/* core-local includes */
#include <native_cpu_component.h>
#include <cpu_session_component.h>

using namespace Genode;


void Native_cpu_component::thread_type(Thread_capability thread_cap,
                                       Thread_type thread_type,
                                       Exception_base exception_base)
{
	auto lambda = [&] (Cpu_thread_component *thread) {
		if (!thread)
			return;

		thread->platform_thread().thread_type(thread_type, exception_base);
	};

	_thread_ep.apply(thread_cap, lambda);
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
