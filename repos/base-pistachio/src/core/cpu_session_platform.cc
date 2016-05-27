/*
 * \brief   Platform-specific parts of the core CPU session interface
 * \author  Martin Stein
 * \date    2012-04-17
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <cpu_session_component.h>

using namespace Genode;
using namespace Pistachio;


Dataspace_capability Cpu_thread_component::utcb()
{
	PERR("%s: Not implemented", __PRETTY_FUNCTION__);
	return Dataspace_capability();
}


Cpu_session::Quota Cpu_session_component::quota() { return Quota(); }
