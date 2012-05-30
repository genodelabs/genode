/*
 * \brief   Platform specific parts of CPU session
 * \author  Martin Stein
 * \date    2012-04-17
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <dataspace/capability.h>

/* core includes */
#include <cpu_session_component.h>

using namespace Genode;


Ram_dataspace_capability
Cpu_session_component::utcb(Thread_capability thread_cap)
{
	/* serialize access */
	Lock::Guard lock_guard(_thread_list_lock);

	/* lookup requested UTCB dataspace */
	Cpu_thread_component * t = _lookup_thread(thread_cap);
	if (!t) return Ram_dataspace_capability();
	return t->platform_thread()->utcb();
}

