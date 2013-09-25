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
	Object_pool<Cpu_thread_component>::Guard
		thread(_thread_ep->lookup_and_lock(thread_cap));
	if (!thread || !thread->platform_thread())
		return Native_capability::invalid_cap();

	return thread->platform_thread()->pause();
}


void
Cpu_session_component::single_step(Thread_capability thread_cap, bool enable)
{
	using namespace Genode;

	Object_pool<Cpu_thread_component>::Guard
		thread(_thread_ep->lookup_and_lock(thread_cap));
	if (!thread || !thread->platform_thread())
		return;

	thread->platform_thread()->single_step(enable);
}
