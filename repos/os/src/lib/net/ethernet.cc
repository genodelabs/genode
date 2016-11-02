/*
 * \brief  Ethernet protocol
 * \author Stefan Kalkowski
 * \date   2010-08-19
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <net/ethernet.h>
#include <net/arp.h>
#include <net/ipv4.h>
#include <base/output.h>

const Net::Mac_address Net::Ethernet_frame::BROADCAST(0xFF);


void Net::Ethernet_frame::print(Genode::Output &output) const
{
	Genode::print(output, "\033[32mETH\033[0m ", src(), " > ", dst(), " ");
	switch (type()) {
	case Ethernet_frame::ARP:
		Genode::print(output,
		              *reinterpret_cast<Arp_packet const *>(data<void>()));
		break;
	case Ethernet_frame::IPV4:
		Genode::print(output,
		              *reinterpret_cast<Ipv4_packet const *>(data<void>()));
		break;
	default: ; }
}
