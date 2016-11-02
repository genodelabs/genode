/*
 * \brief  DHCP related definitions
 * \author Stefan Kalkowski
 * \date   2010-08-19
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode */
#include <net/dhcp.h>
#include <base/output.h>


void Net::Dhcp_packet::print(Genode::Output &output) const
{
	Genode::print(output, "\033[32mDHCP\033[0m ", client_mac(),
	              " > ", siaddr(), " cmd ", op());
}
