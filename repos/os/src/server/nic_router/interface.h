/*
 * \brief  A net interface in form of a signal-driven NIC-packet handler
 * \author Martin Stein
 * \date   2016-08-24
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INTERFACE_H_
#define _INTERFACE_H_

/* local includes */
#include <link.h>
#include <arp_cache.h>
#include <arp_waiter.h>

/* Genode includes */
#include <nic_session/nic_session.h>

namespace Net {

	using Packet_descriptor    = ::Nic::Packet_descriptor;
	using Packet_stream_sink   = ::Nic::Packet_stream_sink< ::Nic::Session::Policy>;
	using Packet_stream_source = ::Nic::Packet_stream_source< ::Nic::Session::Policy>;
	class Forward_rule_tree;
	class Transport_rule_list;
	class Ethernet_frame;
	class Arp_packet;
	class Interface;
	class Configuration;
	class Domain;
}


class Net::Interface
{
	protected:

		using Signal_handler = Genode::Signal_handler<Interface>;

		Signal_handler    _sink_ack;
		Signal_handler    _sink_submit;
		Signal_handler    _source_ack;
		Signal_handler    _source_submit;
		Mac_address const _router_mac;
		Mac_address const _mac;

	private:

		Genode::Timer      &_timer;
		Genode::Allocator  &_alloc;
		Domain             &_domain;
		Arp_cache           _arp_cache;
		Arp_waiter_list     _own_arp_waiters;
		Arp_waiter_list     _foreign_arp_waiters;
		Link_side_tree      _tcp_links;
		Link_side_tree      _udp_links;
		Link_list           _closed_tcp_links;
		Link_list           _closed_udp_links;

		void _new_link(Genode::uint8_t               const  protocol,
		               Link_side_id                  const &local_id,
		               Pointer<Port_allocator_guard> const  remote_port_alloc,
		               Interface                           &remote_interface,
		               Link_side_id                  const &remote_id);

		Forward_rule_tree &_forward_rules(Genode::uint8_t const prot) const;

		Transport_rule_list &_transport_rules(Genode::uint8_t const prot) const;

		void _handle_arp(Ethernet_frame &eth, Genode::size_t const eth_size);

		void _handle_arp_reply(Arp_packet &arp);

		void _handle_arp_request(Ethernet_frame       &eth,
		                         Genode::size_t const  eth_size,
		                         Arp_packet           &arp);

		void _handle_ip(Ethernet_frame          &eth,
		                Genode::size_t    const  eth_size,
		                Packet_descriptor const &pkt);

		void _adapt_eth(Ethernet_frame          &eth,
		                Genode::size_t    const  eth_size,
		                Ipv4_address      const &ip,
		                Packet_descriptor const &pkt,
		                Interface               &interface);

		void _nat_link_and_pass(Ethernet_frame         &eth,
		                        Genode::size_t   const  eth_size,
		                        Ipv4_packet            &ip,
		                        Genode::uint8_t  const  prot,
		                        void            *const  prot_base,
		                        Genode::size_t   const  prot_size,
		                        Link_side_id     const &local_id,
		                        Interface              &interface);

		void _broadcast_arp_request(Ipv4_address const &ip);

		void _send(Ethernet_frame &eth, Genode::size_t const eth_size);

		void _pass_ip(Ethernet_frame         &eth,
		              Genode::size_t   const  eth_size,
		              Ipv4_packet            &ip,
		              Genode::uint8_t  const  prot,
		              void            *const  prot_base,
		              Genode::size_t   const  prot_size);

		void _continue_handle_eth(Packet_descriptor const &pkt);

		Link_list &_closed_links(Genode::uint8_t const protocol);

		Link_side_tree &_links(Genode::uint8_t const protocol);

		Configuration &_config() const;

		Ipv4_address const &_router_ip() const;

		void _handle_eth(void              *const  eth_base,
		                 Genode::size_t     const  eth_size,
		                 Packet_descriptor  const &pkt);

		void _ack_packet(Packet_descriptor const &pkt);

		void _cancel_arp_waiting(Arp_waiter &waiter);

		virtual Packet_stream_sink &_sink() = 0;

		virtual Packet_stream_source &_source() = 0;


		/***********************************
		 ** Packet-stream signal handlers **
		 ***********************************/

		void _ready_to_submit();
		void _ack_avail() { }
		void _ready_to_ack();
		void _packet_avail() { }

	public:

		struct Bad_transport_protocol : Genode::Exception { };
		struct Bad_network_protocol   : Genode::Exception { };
		struct Packet_postponed       : Genode::Exception { };

		Interface(Genode::Entrypoint &ep,
		          Genode::Timer      &timer,
		          Mac_address const   router_mac,
		          Genode::Allocator  &alloc,
		          Mac_address const   mac,
		          Domain             &domain);

		~Interface();

		void link_closed(Link &link, Genode::uint8_t const prot);

		void dissolve_link(Link_side &link_side, Genode::uint8_t const prot);


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;


		/***************
		 ** Accessors **
		 ***************/

		Arp_waiter_list &own_arp_waiters()     { return _own_arp_waiters; }
		Arp_waiter_list &foreign_arp_waiters() { return _foreign_arp_waiters; }
};

#endif /* _INTERFACE_H_ */
