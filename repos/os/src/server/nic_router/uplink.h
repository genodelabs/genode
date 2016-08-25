/*
 * \brief  Uplink interface in form of a NIC session component
 * \author Martin Stein
 * \date   2016-08-23
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _UPLINK_H_
#define _UPLINK_H_

/* Genode includes */
#include <nic_session/connection.h>

/* local includes */
#include <interface.h>

namespace Net {

	class Port_allocator;
	class Uplink;
}

class Net::Uplink : public Nic::Packet_allocator,
                    public Nic::Connection, public Net::Interface
{
	private:

		enum {
			PKT_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE,
			BUF_SIZE = Nic::Session::QUEUE_SIZE * PKT_SIZE,
		};

		Ipv4_address _read_src();

	public:

		Uplink(Server::Entrypoint &ep,
		       Port_allocator     &tcp_port_alloc,
		       Port_allocator     &udp_port_alloc,
		       Tcp_proxy_list     &tcp_proxys,
		       Udp_proxy_list     &udp_proxys,
		       unsigned            rtt_sec,
		       Interface_tree     &interface_tree,
		       Arp_cache          &arp_cache,
		       Arp_waiter_list    &arp_waiters,
		       bool                verbose);


		/********************
		 ** Net::Interface **
		 ********************/

		Packet_stream_sink<Nic::Session::Policy>   *sink()   { return rx(); }
		Packet_stream_source<Nic::Session::Policy> *source() { return tx(); }
};

#endif /* _UPLINK_H_ */
