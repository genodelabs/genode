/*
 * \brief  State tracking for ICMP/UDP//TCP connections
 * \author Martin Stein
 * \date   2016-08-19
 *
 * A link is in the UDP/ICMP case the state tracking of a pseudo UDP/ICMP
 * connection (UDP/ICMP hole punching) and in the TCP case the state tracking
 * of a TCP connection. Beside the layer-3 connection state, a link also
 * contains information about the routing and the NAT translation that
 * correspond to the connection. Link objects have three different functions:
 *
 * 1) Link objects allow the router to manage the lifetime of resources
 *    related to a layer-3 connection.
 *
 * 2) Link objects allow the router to route the back-channel packets of a
 *    connection without additional routing rules.
 *
 * 3) Link objects reduce the routing overhead for successive packets of a
 *    connection as they gather the required information in one place and as
 *    preprocessed as possible.
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LINK_H_
#define _LINK_H_

/* Genode includes */
#include <timer_session/connection.h>
#include <util/avl_tree.h>
#include <util/list.h>
#include <net/ipv4.h>
#include <net/port.h>

/* local includes */
#include <list.h>
#include <reference.h>
#include <pointer.h>
#include <l3_protocol.h>

namespace Net {

	class  Configuration;
	class  Port_allocator_guard;
	class  Tcp_packet;
	class  Domain;
	class  Interface;
	class  Link_side_id;
	class  Link_side;
	class  Link_side_tree;
	class  Link;
	struct Link_list : List<Link> { };
	class  Tcp_link;
	class  Udp_link;
	class  Icmp_link;
}


struct Net::Link_side_id
{
	Ipv4_address const src_ip;
	Port         const src_port;
	Ipv4_address const dst_ip;
	Port         const dst_port;

	static constexpr Genode::size_t data_size();

	void *data_base() const { return (void *)&src_ip; }


	/************************
	 ** Standard operators **
	 ************************/

	bool operator == (Link_side_id const &id) const;

	bool operator > (Link_side_id const &id) const;
}
__attribute__((__packed__));


class Net::Link_side : public Genode::Avl_node<Link_side>
{
	friend class Link;

	private:

		Reference<Domain>   _domain;
		Link_side_id const  _id;
		Link               &_link;

	public:

		Link_side(Domain             &domain,
		          Link_side_id const &id,
		          Link               &link);

		Link_side const &find_by_id(Link_side_id const &id) const;

		bool is_client() const;


		/**************
		 ** Avl_node **
		 **************/

		bool higher(Link_side *side) { return side->_id > _id; }


		/*********
		 ** Log **
		 *********/

		void print(Genode::Output &output) const;


		/***************
		 ** Accessors **
		 ***************/

		Domain             &domain()    const { return _domain(); }
		Link               &link()      const { return _link; }
		Ipv4_address const &src_ip()    const { return _id.src_ip; }
		Ipv4_address const &dst_ip()    const { return _id.dst_ip; }
		Port                src_port()  const { return _id.src_port; }
		Port                dst_port()  const { return _id.dst_port; }
};


struct Net::Link_side_tree : Genode::Avl_tree<Link_side>
{
	struct No_match : Genode::Exception { };

	Link_side const &find_by_id(Link_side_id const &id) const;
};


class Net::Link : public Link_list::Element
{
	protected:

		Reference<Configuration>       _config;
		Interface                     &_client_interface;
		Pointer<Port_allocator_guard>  _server_port_alloc;
		Timer::One_shot_timeout<Link>  _dissolve_timeout;
		Genode::Microseconds           _dissolve_timeout_us;
		L3_protocol             const  _protocol;
		Link_side                      _client;
		Link_side                      _server;

		void _handle_dissolve_timeout(Genode::Duration);

		void _packet() { _dissolve_timeout.schedule(_dissolve_timeout_us); }

	public:

		struct No_port_allocator : Genode::Exception { };

		Link(Interface                           &cln_interface,
		     Link_side_id                  const &cln_id,
		     Pointer<Port_allocator_guard>        srv_port_alloc,
		     Domain                              &srv_domain,
		     Link_side_id                  const &srv_id,
		     Timer::Connection                   &timer,
		     Configuration                       &config,
		     L3_protocol                   const  protocol,
		     Genode::Microseconds          const  dissolve_timeout);

		void dissolve();

		void handle_config(Domain                        &cln_domain,
		                   Domain                        &srv_domain,
		                   Pointer<Port_allocator_guard>  srv_port_alloc,
		                   Configuration                 &config);

		/*********
		 ** Log **
		 *********/

		void print(Genode::Output &output) const;


		/***************
		 ** Accessors **
		 ***************/

		Link_side     &client()         { return _client; }
		Link_side     &server()         { return _server; }
		Configuration &config()         { return _config(); }
		L3_protocol    protocol() const { return _protocol; }
};


class Net::Tcp_link : public Link
{
	private:

		enum class State : Genode::uint8_t { OPEN, CLOSING, CLOSED, };

		struct Peer
		{
			bool fin       { false };
			bool fin_acked { false };
		};

		State _state  { State::OPEN };
		Peer  _client { };
		Peer  _server { };

		void _tcp_packet(Tcp_packet &tcp,
		                 Peer       &sender,
		                 Peer       &receiver);

	public:

		Tcp_link(Interface                     &cln_interface,
		         Link_side_id            const &cln_id,
		         Pointer<Port_allocator_guard>  srv_port_alloc,
		         Domain                        &srv_domain,
		         Link_side_id            const &srv_id,
		         Timer::Connection             &timer,
		         Configuration                 &config,
		         L3_protocol             const  protocol);

		void client_packet(Tcp_packet &tcp) { _tcp_packet(tcp, _client, _server); }

		void server_packet(Tcp_packet &tcp) { _tcp_packet(tcp, _server, _client); }
};


struct Net::Udp_link : Link
{
	Udp_link(Interface                     &cln_interface,
	         Link_side_id            const &cln_id,
	         Pointer<Port_allocator_guard>  srv_port_alloc,
	         Domain                        &srv_domain,
	         Link_side_id            const &srv_id,
	         Timer::Connection             &timer,
	         Configuration                 &config,
	         L3_protocol             const  protocol);

	void packet() { _packet(); }
};


struct Net::Icmp_link : Link
{
	Icmp_link(Interface                     &cln_interface,
	          Link_side_id            const &cln_id,
	          Pointer<Port_allocator_guard>  srv_port_alloc,
	          Domain                        &srv_domain,
	          Link_side_id            const &srv_id,
	          Timer::Connection             &timer,
	          Configuration                 &config,
	          L3_protocol             const  protocol);

	void packet() { _packet(); }
};

#endif /* _LINK_H_ */
