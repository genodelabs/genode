/*
 * \brief  Address resolution protocol
 * \author Stefan Kalkowski
 * \date   2010-08-24
 *
 * ARP is used to determine a network host's link layer or
 * hardware address when only its Network Layer address is known.
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <net/arp.h>
#include <base/output.h>


void Net::Arp_packet::print(Genode::Output &output) const
{
		if (!ethernet_ipv4()) { return; }
		Genode::print(output, "ARP ", src_mac(), " ", src_ip(),
		              " > ", dst_mac(), " ", dst_ip(), " cmd ", opcode());
}
