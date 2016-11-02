/*
 * \brief  User datagram protocol.
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
#include <net/udp.h>
#include <net/dhcp.h>
#include <base/output.h>


void Net::Udp_packet::print(Genode::Output &output) const
{
	Genode::print(output, "\033[32mUDP\033[0m ", src_port(),
	              " > ", dst_port(), " ");
	if (Dhcp_packet::is_dhcp(this)) {
		Genode::print(output,
		              *reinterpret_cast<Dhcp_packet const *>(data<void>()));
	}
}
