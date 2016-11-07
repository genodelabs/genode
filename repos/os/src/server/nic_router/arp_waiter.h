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

/* Genode includes */
#include <net/ipv4.h>
#include <nic_session/nic_session.h>
#include <util/list.h>

#ifndef _ARP_WAITER_H_
#define _ARP_WAITER_H_

namespace Net {

	using ::Nic::Packet_descriptor;
	class Interface;
	class Ethernet_frame;
	class Arp_waiter;
	class Arp_cache_entry;
	using Arp_waiter_list = Genode::List<Arp_waiter>;
}

class Net::Arp_waiter : public Genode::List<Arp_waiter>::Element
{
	private:

		Interface            &_interface;
		Ipv4_address          _ip_addr;
		Ethernet_frame       &_eth;
		Genode::size_t const  _eth_size;
		Packet_descriptor    &_packet;

	public:

		Arp_waiter(Interface &interface, Ipv4_address ip_addr,
		           Ethernet_frame &eth, Genode::size_t const eth_size,
		           Packet_descriptor &packet);

		bool new_arp_cache_entry(Arp_cache_entry &entry);


		/***************
		 ** Accessors **
		 ***************/

		Interface      &interface() const { return _interface; }
		Ethernet_frame &eth()       const { return _eth; }
		Genode::size_t  eth_size()  const { return _eth_size; }
};

#endif /* _ARP_WAITER_H_ */
