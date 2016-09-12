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
#include <interface.h>

using namespace Net;
using namespace Genode;


Arp_waiter::Arp_waiter(Interface               &src,
                       Interface               &dst,
                       Ipv4_address      const &ip,
                       Packet_descriptor const &packet)
:
	_src_le(this), _src(src), _dst_le(this), _dst(dst), _ip(ip),
	_packet(packet)
{
	_src.own_arp_waiters().insert(&_src_le);
	_dst.foreign_arp_waiters().insert(&_dst_le);
}


Arp_waiter::~Arp_waiter()
{
	_src.own_arp_waiters().remove(&_src_le);
	_dst.foreign_arp_waiters().remove(&_dst_le);
}
