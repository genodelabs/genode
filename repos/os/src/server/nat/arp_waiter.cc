/*
 * \brief  Aspect of waiting for an ARP reply
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* local includes */
#include <arp_waiter.h>
#include <arp_cache.h>
#include <interface.h>

using namespace Net;
using namespace Genode;


Arp_waiter::Arp_waiter(Interface &interface, Ipv4_address ip_addr,
                       Ethernet_frame &eth, size_t const eth_size,
                       Packet_descriptor &packet)
:
	_interface(interface), _ip_addr(ip_addr), _eth(eth), _eth_size(eth_size),
	_packet(packet)
{ }


bool Arp_waiter::new_arp_cache_entry(Arp_cache_entry &entry)
{
	if (!(entry.ip_addr() == _ip_addr)) { return false; }
	_interface.continue_handle_ethernet(&_eth, _eth_size, &_packet);
	return true;
}
