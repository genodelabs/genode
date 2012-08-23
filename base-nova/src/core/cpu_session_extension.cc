/**
 * \brief  Core implementation of the CPU session interface extension
 * \author Alexander Boettcher
 * \date   2012-07-27
 */

/*
 * Copyright (C) 2012-2012 Genode Labs GmbH
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
Cpu_session_component::native_cap(Thread_capability thread_cap)
{
	Cpu_thread_component *thread = _lookup_thread(thread_cap);
	if (!thread || !thread->platform_thread())
		return Native_capability::invalid_cap();

	return thread->platform_thread()->native_cap();
}

Native_capability
Cpu_session_component::pause_sync(Thread_capability target_thread_cap)
{
	Cpu_thread_component *thread = _lookup_thread(target_thread_cap);
	if (!thread || !thread->platform_thread())
		return Native_capability::invalid_cap();

	return thread->platform_thread()->pause();
}

void
Cpu_session_component::single_step(Thread_capability thread_cap, bool enable)
{
	using namespace Genode;

	Cpu_thread_component *thread = _lookup_thread(thread_cap);
	if (!thread || !thread->platform_thread())
		return;

	thread->platform_thread()->single_step(enable);
}
