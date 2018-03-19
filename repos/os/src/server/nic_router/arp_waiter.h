/*
 * \brief  Remember packets that wait for ARP replies at different interfaces
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _ARP_WAITER_H_
#define _ARP_WAITER_H_

/* local includes */
#include <list.h>
#include <reference.h>

/* Genode includes */
#include <net/ipv4.h>
#include <util/list.h>
#include <nic_session/nic_session.h>

namespace Net {

	using Packet_descriptor = ::Nic::Packet_descriptor;
	class Interface;
	class Domain;
	class Configuration;
	class Arp_waiter;
	using Arp_waiter_list_element = Genode::List_element<Arp_waiter>;
	using Arp_waiter_list         = List<Arp_waiter_list_element>;
}


class Net::Arp_waiter
{
	private:

		Arp_waiter_list_element  _src_le;
		Interface               &_src;
		Arp_waiter_list_element  _dst_le;
		Reference<Domain>        _dst;
		Ipv4_address      const  _ip;
		Packet_descriptor const  _packet;

	public:

		Arp_waiter(Interface               &src,
		           Domain                  &dst,
		           Ipv4_address      const &ip,
		           Packet_descriptor const &packet);

		~Arp_waiter();

		void handle_config(Domain &dst);


		/*********
		 ** Log **
		 *********/

		void print(Genode::Output &output) const;


		/***************
		 ** Accessors **
		 ***************/

		Interface               &src()    const { return _src; }
		Ipv4_address      const &ip()     const { return _ip; }
		Packet_descriptor const &packet() const { return _packet; }
		Domain                  &dst()          { return _dst(); }
};

#endif /* _ARP_WAITER_H_ */
