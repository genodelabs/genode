/*
 * \brief  Core implementation of the CPU session interface
 * \author Norman Feske
 * \date   2012-08-15
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core-local includes */
#include <cpu_session_component.h>
#include <native_cpu_component.h>

/* base-internal includes */
#include <base/internal/capability_space_tpl.h>

using namespace Genode;


void Native_cpu_component::thread_id(Thread_capability thread_cap, int pid, int tid)
{
	_thread_ep.apply(thread_cap, [&] (Cpu_thread_component *thread) {
		if (thread) thread->platform_thread().thread_id(pid, tid); });
}


Untyped_capability Native_cpu_component::server_sd(Thread_capability thread_cap)
{
	auto lambda = [] (Cpu_thread_component *thread) {
		if (!thread) return Untyped_capability();

		return Capability_space::import(Rpc_destination(thread->platform_thread().server_sd()),
		                                Rpc_obj_key());
	};
	return _thread_ep.apply(thread_cap, lambda);
}


Untyped_capability Native_cpu_component::client_sd(Thread_capability thread_cap)
{
	auto lambda = [] (Cpu_thread_component *thread) {
		if (!thread) return Untyped_capability();

		return Capability_space::import(Rpc_destination(thread->platform_thread().client_sd()),
		                                Rpc_obj_key());
	};
	return _thread_ep.apply(thread_cap, lambda);
}


Native_cpu_component::Native_cpu_component(Cpu_session_component &cpu_session, char const *)
:
	_cpu_session(cpu_session), _thread_ep(*_cpu_session._thread_ep)
{
	_thread_ep.manage(this);
}


Native_cpu_component::~Native_cpu_component()
{
	_thread_ep.dissolve(this);
}

