/*
 * \brief  A net interface in form of a signal-driven NIC-packet handler
 * \author Martin Stein
 * \date   2016-08-24
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INTERFACE_H_
#define _INTERFACE_H_

/* Genode includes */
#include <os/server.h>
#include <os/session_policy.h>
#include <util/avl_string.h>
#include <nic_session/nic_session.h>

/* local includes */
#include <ip_route.h>

namespace Net {

	using ::Nic::Packet_stream_sink;
	using ::Nic::Packet_stream_source;
	using ::Nic::Packet_descriptor;

	class Ethernet_frame;
	class Arp_packet;
	class Arp_waiter;
	class Arp_cache;
	class Arp_cache_entry;
	class Port_allocator;
	class Tcp_proxy;
	class Udp_proxy;
	class Interface;
	class Interface_tree;

	using Interface_list    = Genode::List<Interface>;
	using Arp_waiter_list   = Genode::List<Arp_waiter>;
	using Tcp_proxy_list    = Genode::List<Tcp_proxy>;
	using Udp_proxy_list    = Genode::List<Udp_proxy>;
	using Signal_rpc_member = Genode::Signal_rpc_member<Interface>;
}


class Net::Interface : public Genode::Session_label, public Genode::Avl_string_base
{
	protected:

		Signal_rpc_member _sink_ack;
		Signal_rpc_member _sink_submit;
		Signal_rpc_member _source_ack;
		Signal_rpc_member _source_submit;

	private:

		Packet_descriptor       _packet;
		Genode::Entrypoint     &_ep;
		Ip_route_list           _ip_routes;
		Mac_address const       _router_mac;
		Ipv4_address const      _router_ip;
		Mac_address const       _mac;
		Genode::Allocator      &_allocator;
		Genode::Session_policy  _policy;
		bool const              _proxy;
		unsigned                _tcp_proxy;
		unsigned                _tcp_proxy_used;
		Tcp_proxy_list         &_tcp_proxies;
		Port_allocator         &_tcp_port_alloc;
		unsigned                _udp_proxy;
		unsigned                _udp_proxy_used;
		Udp_proxy_list         &_udp_proxies;
		Port_allocator         &_udp_port_alloc;
		unsigned const          _rtt_sec;
		Interface_tree         &_interface_tree;
		Arp_cache              &_arp_cache;
		Arp_waiter_list        &_arp_waiters;
		bool                    _verbose;

		void _read_route(Genode::Xml_node &route_xn);

		Tcp_proxy *_find_tcp_proxy_by_client(Ipv4_address ip,
		                                     Genode::uint16_t port);

		Udp_proxy *_find_udp_proxy_by_client(Ipv4_address ip,
		                                     Genode::uint16_t port);

		Interface *_tlp_proxy_route(Genode::uint8_t tlp, void *ptr,
		                            Genode::uint16_t &dst_port,
		                            Ipv4_packet *ip, Ipv4_address &to,
		                            Ipv4_address &via);

		void _tlp_apply_port_proxy(Genode::uint8_t tlp, void *tlp_ptr,
		                           Ipv4_packet *ip, Ipv4_address client_ip,
		                           Genode::uint16_t src_port);

		Tcp_proxy *_find_tcp_proxy_by_proxy(Ipv4_address ip,
		                                    Genode::uint16_t port);

		Udp_proxy *_find_udp_proxy_by_proxy(Ipv4_address ip,
		                                    Genode::uint16_t port);

		void _handle_arp_reply(Arp_packet * const arp);

		void _handle_arp_request(Ethernet_frame * const eth,
		                         Genode::size_t const eth_size,
		                         Arp_packet * const arp);

		Arp_waiter *_new_arp_entry(Arp_waiter *arp_waiter,
		                           Arp_cache_entry *entry);

		void _remove_arp_waiter(Arp_waiter *arp_waiter);

		void _handle_arp(Ethernet_frame *eth, Genode::size_t size);

		void _handle_ip(Ethernet_frame *eth, Genode::size_t eth_size,
		                bool &ack_packet, Packet_descriptor *packet);

		Tcp_proxy *_new_tcp_proxy(unsigned const client_port,
		                          Ipv4_address client_ip,
		                          Ipv4_address proxy_ip);

		Udp_proxy *_new_udp_proxy(unsigned const client_port,
		                          Ipv4_address client_ip,
		                          Ipv4_address proxy_ip);

		void _delete_tcp_proxy(Tcp_proxy * const proxy);

		bool _chk_delete_tcp_proxy(Tcp_proxy * &proxy);

		void _delete_udp_proxy(Udp_proxy * const proxy);

		bool _chk_delete_udp_proxy(Udp_proxy * &proxy);


		/***********************************
		 ** Packet-stream signal handlers **
		 ***********************************/

		void _ready_to_submit(unsigned);
		void _ack_avail(unsigned) { }
		void _ready_to_ack(unsigned);
		void _packet_avail(unsigned) { }

	public:

		struct Too_many_tcp_proxies : Genode::Exception { };
		struct Too_many_udp_proxies : Genode::Exception { };

		Interface(Server::Entrypoint    &ep,
		          Mac_address const      router_mac,
		          Ipv4_address const     router_ip,
		          Genode::Allocator     &allocator,
		          char const            *args,
		          Port_allocator        &tcp_port_alloc,
		          Port_allocator        &udp_port_alloc,
		          Mac_address const      mac,
		          Tcp_proxy_list        &tcp_proxies,
		          Udp_proxy_list        &udp_proxies,
		          unsigned const         rtt_sec,
		          Interface_tree        &interface_tree,
		          Arp_cache             &arp_cache,
		          Arp_waiter_list       &arp_waiters,
		          bool                   verbose);

		~Interface();

		void arp_broadcast(Ipv4_address ip_addr);

		void send(Ethernet_frame *eth, Genode::size_t eth_size);

		void handle_ethernet(void *src, Genode::size_t size, bool &ack,
		                     Packet_descriptor *packet);

		void continue_handle_ethernet(void *src, Genode::size_t size,
		                              Packet_descriptor *packet);


		/***************
		 ** Accessors **
		 ***************/

		Mac_address        router_mac() const { return _router_mac; }
		Mac_address        mac()        const { return _mac; }
		Ipv4_address       router_ip()  const { return _router_ip; }
		Ip_route_list     &ip_routes()        { return _ip_routes; }
		Genode::Allocator &allocator()  const { return _allocator; }
		Session_label     &label()            { return *this; }

		virtual Packet_stream_sink< ::Nic::Session::Policy>   *sink()   = 0;
		virtual Packet_stream_source< ::Nic::Session::Policy> *source() = 0;
};


struct Net::Interface_tree : Genode::Avl_tree<Genode::Avl_string_base>
{
	Interface *find_by_label(char const *label);
};

#endif /* _INTERFACE_H_ */
