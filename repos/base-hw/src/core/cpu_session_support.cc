/*
 * \brief   Platform specific parts of CPU session
 * \author  Martin Stein
 * \date    2012-04-17
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <dataspace/capability.h>

/* core includes */
#include <cpu_session_component.h>
#include <kernel/configuration.h>

using namespace Genode;


Dataspace_capability Cpu_thread_component::utcb()
{
	return _platform_thread.utcb();
}


Cpu_session::Quota Cpu_session_component::quota()
{
	size_t const spu = Kernel::cpu_quota_us;
	size_t const u = quota_lim_downscale<sizet_arithm_t>(_quota, spu);
	return { spu, u };
}
