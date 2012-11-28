/*
 * \brief   Platform specific parts of CPU session
 * \author  Martin Stein
 * \date    2012-11-27
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


Ram_dataspace_capability Cpu_session_component::utcb(Thread_capability) {
	PDBG("Not implemented");
	return Ram_dataspace_capability();
};

