/*
 * \brief  Nic-bridge global environment
 * \author Stefan Kalkowski
 * \date   2013-05-23
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "env.h"
#include "nic.h"
#include "vlan.h"

Genode::Signal_receiver* Net::Env::receiver()
{
	static Genode::Signal_receiver receiver;
	return &receiver;
}


Net::Vlan* Net::Env::vlan()
{
	static Net::Vlan vlan;
	return &vlan;
}


Net::Nic* Net::Env::nic()
{
	static Net::Nic nic;
	return &nic;
}
