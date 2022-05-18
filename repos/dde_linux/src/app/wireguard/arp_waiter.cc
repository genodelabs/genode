/*
 * \brief  Aspect of waiting for an ARP reply
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <arp_waiter.h>

using namespace Net;
using namespace Genode;


Arp_waiter::Arp_waiter(Arp_waiter_list         &list,
                       Ipv4_address      const &ip,
                       Packet_descriptor const &packet)
:
	_list(list), _le(this), _ip(ip), _packet(packet)
{
	_list.insert(&_le);
}


Arp_waiter::~Arp_waiter()
{
	_list.remove(&_le);
}


void Arp_waiter::print(Output &output) const
{
	Genode::print(output, "IP ", _ip);
}
