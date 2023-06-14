/*
 * \brief  Ethernet protocol
 * \author Stefan Kalkowski
 * \date   2010-08-19
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <net/ethernet.h>
#include <net/arp.h>
#include <net/ipv4.h>
#include <base/output.h>


void Net::Ethernet_frame::print(Genode::Output &output) const
{
	Genode::print(output, "ETH ", src(), " > ", dst(), " ");
	switch (type()) {
	case Ethernet_frame::Type::ARP:
		Genode::print(output, *reinterpret_cast<Arp_packet const *>(_data));
		break;
	case Ethernet_frame::Type::IPV4:
		Genode::print(output, *reinterpret_cast<Ipv4_packet const *>(_data));
		break;
	default: ; }
}
