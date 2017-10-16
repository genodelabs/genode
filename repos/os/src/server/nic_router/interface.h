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
#include <l3_protocol.h>
#include <dhcp_client.h>
#include <dhcp_server.h>

/* Genode includes */
#include <nic_session/nic_session.h>
#include <net/dhcp.h>

namespace Net {

	using Packet_descriptor    = ::Nic::Packet_descriptor;
	using Packet_stream_sink   = ::Nic::Packet_stream_sink< ::Nic::Session::Policy>;
	using Packet_stream_source = ::Nic::Packet_stream_source< ::Nic::Session::Policy>;
	class Ipv4_config;
	class Forward_rule_tree;
	class Transport_rule_list;
	class Ethernet_frame;
	class Arp_packet;
	class Interface;
	class Dhcp_server;
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

		void _init();

	private:

		Timer::Connection    &_timer;
		Genode::Allocator    &_alloc;
		Domain               &_domain;
		Arp_cache             _arp_cache;
		Arp_waiter_list       _own_arp_waiters;
		Arp_waiter_list       _foreign_arp_waiters;
		Link_side_tree        _tcp_links;
		Link_side_tree        _udp_links;
		Link_list             _closed_tcp_links;
		Link_list             _closed_udp_links;
		Dhcp_allocation_tree  _dhcp_allocations;
		Dhcp_allocation_list  _released_dhcp_allocations;
		Dhcp_client           _dhcp_client { _alloc, _timer, *this };

		void _new_link(L3_protocol                   const  protocol,
		               Link_side_id                  const &local_id,
		               Pointer<Port_allocator_guard> const  remote_port_alloc,
		               Interface                           &remote_interface,
		               Link_side_id                  const &remote_id);

		void _destroy_released_dhcp_allocations();

		void _destroy_dhcp_allocation(Dhcp_allocation &allocation);

		void _release_dhcp_allocation(Dhcp_allocation &allocation);

		void _send_dhcp_reply(Dhcp_server               const &dhcp_srv,
		                      Mac_address               const &client_mac,
		                      Ipv4_address              const &client_ip,
		                      Dhcp_packet::Message_type        msg_type,
		                      Genode::uint32_t                 xid);

		Forward_rule_tree &_forward_rules(L3_protocol const prot) const;

		Transport_rule_list &_transport_rules(L3_protocol const prot) const;

		void _handle_arp(Ethernet_frame &eth, Genode::size_t const eth_size);

		void _handle_arp_reply(Arp_packet &arp);

		void _handle_arp_request(Ethernet_frame       &eth,
		                         Genode::size_t const  eth_size,
		                         Arp_packet           &arp);

		void _handle_dhcp_request(Ethernet_frame &eth,
		                          Genode::size_t  eth_size,
		                          Dhcp_packet    &dhcp);

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
		                        L3_protocol      const  prot,
		                        void            *const  prot_base,
		                        Genode::size_t   const  prot_size,
		                        Link_side_id     const &local_id,
		                        Interface              &interface);

		void _broadcast_arp_request(Ipv4_address const &ip);

		void _pass_prot(Ethernet_frame         &eth,
		                Genode::size_t   const  eth_size,
		                Ipv4_packet            &ip,
		                L3_protocol      const  prot,
		                void            *const  prot_base,
		                Genode::size_t   const  prot_size);

		void _pass_ip(Ethernet_frame       &eth,
		              Genode::size_t const  eth_size,
		              Ipv4_packet          &ip);

		void _continue_handle_eth(Packet_descriptor const &pkt);

		Link_list &_closed_links(L3_protocol const protocol);

		Link_side_tree &_links(L3_protocol const protocol);

		Configuration &_config() const;

		Ipv4_config const &_ip_config() const;

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

		struct Bad_send_dhcp_args           : Genode::Exception { };
		struct Bad_transport_protocol       : Genode::Exception { };
		struct Bad_network_protocol         : Genode::Exception { };
		struct Packet_postponed             : Genode::Exception { };
		struct Bad_dhcp_request             : Genode::Exception { };
		struct Alloc_dhcp_msg_buffer_failed : Genode::Exception { };
		struct Dhcp_msg_buffer_too_small    : Genode::Exception { };

		struct Packet_ignored : Genode::Exception
		{
			char const *reason;

			Packet_ignored(char const *reason) : reason(reason) { }
		};

		Interface(Genode::Entrypoint &ep,
		          Timer::Connection  &timer,
		          Mac_address const   router_mac,
		          Genode::Allocator  &alloc,
		          Mac_address const   mac,
		          Domain             &domain);

		~Interface();

		void link_closed(Link &link, L3_protocol const prot);

		void dhcp_allocation_expired(Dhcp_allocation &allocation);

		void dissolve_link(Link_side &link_side, L3_protocol const prot);

		void send(Ethernet_frame &eth, Genode::size_t const eth_size);


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;


		/***************
		 ** Accessors **
		 ***************/

		Domain          &domain()              { return _domain; }
		Mac_address      router_mac()    const { return _router_mac; }
		Arp_waiter_list &own_arp_waiters()     { return _own_arp_waiters; }
		Arp_waiter_list &foreign_arp_waiters() { return _foreign_arp_waiters; }
};

#endif /* _INTERFACE_H_ */
