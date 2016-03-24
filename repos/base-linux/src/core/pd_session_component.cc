/*
 * \brief  Core implementation of the PD session interface
 * \author Norman Feske
 * \date   2012-08-15
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core-local includes */
#include <pd_session_component.h>

using namespace Genode;


int Pd_session_component::bind_thread(Thread_capability) { return -1; }


int Pd_session_component::assign_parent(Capability<Parent> parent)
{
	_parent = parent;
	return 0;
}


bool Pd_session_component::assign_pci(addr_t, uint16_t) { return true; }

