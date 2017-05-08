/**
 * \brief  Core implementation of the CPU session interface extension
 * \author Stefan Kalkowski
 * \date   2011-04-04
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

/* core includes */
#include <native_cpu_component.h>
#include <cpu_session_irqs.h>
#include <platform.h>

/* Fiasco.OC includes */
namespace Fiasco {
#include <l4/sys/thread.h>
#include <l4/sys/factory.h>
}


Genode::Native_capability
Genode::Native_cpu_component::native_cap(Genode::Thread_capability cap)
{
	using namespace Genode;

	auto lambda = [&] (Cpu_thread_component *thread) {
		return (!thread) ? Native_capability()
		                 : thread->platform_thread().thread().local;
	};
	return _thread_ep.apply(cap, lambda);
}


Genode::Foc_thread_state
Genode::Native_cpu_component::thread_state(Genode::Thread_capability cap)
{
	using namespace Genode;

	auto lambda = [&] (Cpu_thread_component *thread) {
		return (!thread) ? Foc_thread_state()
		                 : thread->platform_thread().state(); };

	return _thread_ep.apply(cap, lambda);
}


Genode::Native_cpu_component::Native_cpu_component(Cpu_session_component &cpu_session, char const *)
:
	_cpu_session(cpu_session), _thread_ep(*_cpu_session._thread_ep)
{
	_thread_ep.manage(this);
}


Genode::Native_cpu_component::~Native_cpu_component()
{
	_thread_ep.dissolve(this);
}
