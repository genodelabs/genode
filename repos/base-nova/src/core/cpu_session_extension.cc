/**
 * \brief  Core implementation of the CPU session interface extension
 * \author Alexander Boettcher
 * \date   2012-07-27
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/stdint.h>

/* Core includes */
#include <cpu_session_component.h>

using namespace Genode;


Native_capability
Cpu_session_component::pause_sync(Thread_capability thread_cap)
{
	auto lambda = [] (Cpu_thread_component *thread) {
		if (!thread || !thread->platform_thread())
			return Native_capability();

		return thread->platform_thread()->pause();
	};
	return _thread_ep->apply(thread_cap, lambda);
}


Native_capability
Cpu_session_component::single_step_sync(Thread_capability thread_cap, bool enable)
{
	using namespace Genode;

	auto lambda = [enable] (Cpu_thread_component *thread) {
		if (!thread || !thread->platform_thread())
			return Native_capability();

		return thread->platform_thread()->single_step(enable);
	};
	return _thread_ep->apply(thread_cap, lambda);
}


void Cpu_session_component::single_step(Thread_capability, bool) { return; }
