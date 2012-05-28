/*
 * \brief   Platform-specific parts of cores CPU-session interface
 * \author  Martin Stein
 * \date    2012-04-17
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* Core includes */
#include <cpu_session_component.h>

using namespace Genode;


Ram_dataspace_capability Cpu_session_component::utcb(Thread_capability thread_cap)
{
	PERR("%s: Not implemented", __PRETTY_FUNCTION__);
	return Ram_dataspace_capability();
}

