/*
 * \brief   Platform-specific parts of core's CPU-service
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
#include <base/log.h>

/* core includes */
#include <cpu_session_component.h>

using namespace Genode;


Dataspace_capability Cpu_thread_component::utcb()
{
	error(__PRETTY_FUNCTION__, ": not implemented");
	return Dataspace_capability();
}


Cpu_session::Quota Cpu_session_component::quota() { return Quota(); }
