/*
 * \brief  Core implementation of the PD session interface
 * \author Norman Feske
 * \date   2012-08-15
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core-local includes */
#include <pd_session_component.h>

using namespace Genode;


void Pd_session_component::assign_parent(Capability<Parent> parent)
{
	_parent = parent;
}


bool Pd_session_component::assign_pci(addr_t, uint16_t) { return true; }

