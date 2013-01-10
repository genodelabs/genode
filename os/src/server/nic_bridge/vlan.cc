/*
 * \brief  Virtual local network.
 * \author Stefan Kalkowski
 * \date   2010-08-18
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "vlan.h"

Net::Vlan* Net::Vlan::vlan()
{
	static Net::Vlan vlan;
	return &vlan;
}
