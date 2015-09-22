/*
 * \brief  Core implementation of the PD session interface extension
 * \author Stefan Kalkowski
 * \date   2009-06-21
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <util.h>
#include <pd_session_component.h>
#include <cpu_session_component.h>

using namespace Genode;


void Pd_session_component::space_pager(Thread_capability thread)
{
	_thread_ep->apply(thread, [this] (Cpu_thread_component *cpu_thread)
	{
		if (!cpu_thread) return;
		_pd.space_pager(cpu_thread->platform_thread());
	});
}
