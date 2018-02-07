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
#include <arp_waiter.h>
#include <l3_protocol.h>
#include <dhcp_client.h>
#include <dhcp_server.h>
#include <list.h>

/* Genode includes */
#include <nic_session/nic_session.h>
#include <net/dhcp.h>

namespace Net {

	using Packet_descriptor    = ::Nic::Packet_descriptor;
	using Packet_stream_sink   = ::Nic::Packet_stream_sink< ::Nic::Session::Policy>;
	using Packet_stream_source = ::Nic::Packet_stream_source< ::Nic::Session::Policy>;
	using Domain_name          = Genode::String<160>;
	class Ipv4_config;
	class Forward_rule_tree;
	class Transport_rule_list;
	class Ethernet_frame;
	class Arp_packet;
	class Interface_policy;
	class Interface;
	class Dhcp_server;
	class Configuration;
	class Domain;
}


struct Net::Interface_policy
{
	virtual Domain_name determine_domain_name() const = 0;

	virtual ~Interface_policy() { }
};


class Net::Interface : private Genode::List<Interface>::Element
{
	protected:

		friend class Genode::List<Interface>;
		friend class Net::List<Interface>;

		using Signal_handler = Genode::Signal_handler<Interface>;

		Signal_handler      _sink_ack;
		Signal_handler      _sink_submit;
		Signal_handler      _source_ack;
		Signal_handler      _source_submit;
		Mac_address  const  _router_mac;
		Mac_address  const  _mac;

	private:

		Configuration          &_config;
		Interface_policy const &_policy;
		Timer::Connection      &_timer;
		Genode::Allocator      &_alloc;
		Pointer<Domain>         _domain_ptr                { };
		Arp_waiter_list         _own_arp_waiters           { };
		Link_list               _tcp_links                 { };
		Link_list               _udp_links                 { };
		Link_list               _dissolved_tcp_links       { };
		Link_list               _dissolved_udp_links       { };
		Dhcp_allocation_tree    _dhcp_allocations          { };
		Dhcp_allocation_list    _released_dhcp_allocations { };
		Dhcp_client             _dhcp_client               { _alloc, _timer, *this };

		void _new_link(L3_protocol                   const  protocol,
		               Link_side_id                  const &local_id,
		               Pointer<Port_allocator_guard> const  remote_port_alloc,
		               Domain                              &remote_domain,
		               Link_side_id                  const &remote_id);

		void _destroy_released_dhcp_allocations(Domain &local_domain);

		void _destroy_dhcp_allocation(Dhcp_allocation &allocation,
		                              Domain          &local_domain);

		void _release_dhcp_allocation(Dhcp_allocation &allocation,
		                              Domain          &local_domain);

		void _new_dhcp_allocation(Ethernet_frame &eth,
		                          Dhcp_packet    &dhcp,
		                          Dhcp_server    &dhcp_srv,
		                          Domain         &local_domain);

		void _send_dhcp_reply(Dhcp_server               const &dhcp_srv,
		                      Mac_address               const &client_mac,
		                      Ipv4_address              const &client_ip,
		                      Dhcp_packet::Message_type        msg_type,
		                      Genode::uint32_t                 xid,
		                      Ipv4_address_prefix       const &local_intf);

		Forward_rule_tree &_forward_rules(Domain            &local_domain,
		                                  L3_protocol const  prot) const;

		Transport_rule_list &_transport_rules(Domain            &local_domain,
		                                      L3_protocol const  prot) const;

		void _handle_arp(Ethernet_frame       &eth,
		                 Genode::size_t const  eth_size,
		                 Domain               &local_domain);

		void _handle_arp_reply(Ethernet_frame       &eth,
		                       Genode::size_t const  eth_size,
		                       Arp_packet           &arp,
		                       Domain               &local_domain);

		void _handle_arp_request(Ethernet_frame       &eth,
		                         Genode::size_t const  eth_size,
		                         Arp_packet           &arp,
		                         Domain               &local_domain);

		void _send_arp_reply(Ethernet_frame       &eth,
		                     Genode::size_t const  eth_size,
		                     Arp_packet           &arp);

		void _handle_dhcp_request(Ethernet_frame &eth,
		                          Genode::size_t  eth_size,
		                          Dhcp_packet    &dhcp,
		                          Domain         &local_domain);

		void _handle_ip(Ethernet_frame          &eth,
		                Genode::size_t    const  eth_size,
		                Packet_descriptor const &pkt,
		                Domain                  &local_domain);

		void _adapt_eth(Ethernet_frame          &eth,
		                Ipv4_address      const &dst_ip,
		                Packet_descriptor const &pkt,
		                Domain                  &remote_domain);

		void _nat_link_and_pass(Ethernet_frame         &eth,
		                        Genode::size_t   const  eth_size,
		                        Ipv4_packet            &ip,
		                        L3_protocol      const  prot,
		                        void            *const  prot_base,
		                        Genode::size_t   const  prot_size,
		                        Link_side_id     const &local_id,
		                        Domain                 &local_domain,
		                        Domain                 &remote_domain);

		void _broadcast_arp_request(Ipv4_address const &src_ip,
		                            Ipv4_address const &dst_ip);

		void _domain_broadcast(Ethernet_frame &eth,
		                       Genode::size_t  eth_size,
		                       Domain         &local_domain);

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

		Ipv4_address const &_router_ip() const;

		void _handle_eth(void              *const  eth_base,
		                 Genode::size_t     const  eth_size,
		                 Packet_descriptor  const &pkt);

		void _ack_packet(Packet_descriptor const &pkt);

		virtual Packet_stream_sink &_sink() = 0;

		virtual Packet_stream_source &_source() = 0;

		void _send_alloc_pkt(Genode::Packet_descriptor   &pkt,
		                     void                      * &pkt_base,
		                     Genode::size_t               pkt_size);

		void _send_submit_pkt(Genode::Packet_descriptor   &pkt,
		                      void                      * &pkt_base,
		                      Genode::size_t               pkt_size);


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
		struct Alloc_dhcp_msg_buffer_failed : Genode::Exception { };
		struct Dhcp_msg_buffer_too_small    : Genode::Exception { };

		struct Drop_packet_inform : Genode::Exception
		{
			Genode::String<128> msg;

			template <typename... ARGS>
			Drop_packet_inform(ARGS... args) : msg({args...}) { }
		};

		struct Drop_packet_warn : Genode::Exception
		{
			Genode::String<128> msg;

			template <typename... ARGS>
			Drop_packet_warn(ARGS... args) : msg({args...}) { }
		};

		Interface(Genode::Entrypoint     &ep,
		          Timer::Connection      &timer,
		          Mac_address      const  router_mac,
		          Genode::Allocator      &alloc,
		          Mac_address      const  mac,
		          Configuration          &config,
		          Interface_policy const &policy);

		virtual ~Interface();

		void dhcp_allocation_expired(Dhcp_allocation &allocation);

		template <typename FUNC>
		void send(Genode::size_t pkt_size, FUNC && write_to_pkt)
		{
			try {
				Packet_descriptor  pkt;
				void              *pkt_base;

				_send_alloc_pkt(pkt, pkt_base, pkt_size);
				write_to_pkt(pkt_base);
				_send_submit_pkt(pkt, pkt_base, pkt_size);
			}
			catch (Packet_stream_source::Packet_alloc_failed) {
				Genode::warning("failed to allocate packet");
			}
		}

		void send(Ethernet_frame &eth, Genode::size_t eth_size);

		Link_list &dissolved_links(L3_protocol const protocol);

		Link_list &links(L3_protocol const protocol);

		void cancel_arp_waiting(Arp_waiter &waiter);

		void attach_to_domain(Domain &domain);

		void detach_from_domain();


		/***************
		 ** Accessors **
		 ***************/

		Domain          &domain()           { return _domain_ptr.deref(); }
		Mac_address      router_mac() const { return _router_mac; }
		Arp_waiter_list &own_arp_waiters()  { return _own_arp_waiters; }
};

#endif /* _INTERFACE_H_ */
