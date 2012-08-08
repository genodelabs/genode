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
	if (!thread)
		return Native_capability::invalid_cap();

	return thread->platform_thread()->native_cap();
}

int
Cpu_session_component::start_exc_base_vcpu(Thread_capability thread_cap,
                                           addr_t ip, addr_t sp, 
                                           addr_t exc_base, bool vcpu)
{
	Cpu_thread_component *thread = _lookup_thread(thread_cap);
	if (!thread) return -1;

	return thread->platform_thread()->start((void *)ip, (void *)sp,
	                                        exc_base, vcpu);
}
