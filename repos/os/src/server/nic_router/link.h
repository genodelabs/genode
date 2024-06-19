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
#include <util/avl_tree.h>
#include <util/list.h>
#include <net/ipv4.h>
#include <net/port.h>

/* local includes */
#include <list.h>
#include <l3_protocol.h>
#include <lazy_one_shot_timeout.h>

namespace Net {

	class  Interface_link_stats;
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

	bool operator != (Link_side_id const &id) const;

	bool operator > (Link_side_id const &id) const;
}
__attribute__((__packed__));


class Net::Link_side : public Genode::Avl_node<Link_side>
{
	friend class Link;

	private:

		Domain             *_domain_ptr;
		Link_side_id const  _id;
		Link               &_link;

		/*
		 * Noncopyable
		 */
		Link_side(Link_side const &);
		Link_side &operator = (Link_side const &);

	public:

		Link_side(Domain             &domain,
		          Link_side_id const &id,
		          Link               &link);

		void find_by_id(Link_side_id const &id, auto const &handle_match, auto const &handle_no_match) const
		{
			if (id != _id) {

				Link_side *const link_side_ptr {
					Avl_node<Link_side>::child(id > _id) };

				if (link_side_ptr != nullptr) {

					link_side_ptr->find_by_id(
						id, handle_match, handle_no_match);

				} else {

					handle_no_match();
				}

			} else {

				handle_match(*this);
			}
		}

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

		Domain             &domain()    const { return *_domain_ptr; }
		Link               &link()      const { return _link; }
		Ipv4_address const &src_ip()    const { return _id.src_ip; }
		Ipv4_address const &dst_ip()    const { return _id.dst_ip; }
		Port                src_port()  const { return _id.src_port; }
		Port                dst_port()  const { return _id.dst_port; }
};


struct Net::Link_side_tree : Genode::Avl_tree<Link_side>
{
	void find_by_id(Link_side_id const &id, auto const &handle_match, auto const &handle_no_match) const
	{
		if (first() != nullptr) {

			first()->find_by_id(id, handle_match, handle_no_match);

		} else {

			handle_no_match();
		}
	}
};


class Net::Link : public Link_list::Element
{
	protected:

		Configuration                 *_config_ptr;
		Interface                     &_client_interface;
		Port_allocator_guard          *_server_port_alloc_ptr;
		Lazy_one_shot_timeout<Link>    _dissolve_timeout;
		Genode::Microseconds           _dissolve_timeout_us;
		L3_protocol             const  _protocol;
		Link_side                      _client;
		Link_side                      _server;
		bool                           _opening { true };
		Interface_link_stats          &_stats;
		Genode::size_t                *_stats_ptr;

		void _handle_dissolve_timeout(Genode::Duration);

		void _packet() { _dissolve_timeout.schedule(_dissolve_timeout_us); }

		/*
		 * Noncopyable
		 */
		Link(Link const &);
		Link &operator = (Link const &);

	public:

		struct No_port_allocator : Genode::Exception { };

		Link(Interface                  &cln_interface,
		     Domain                     &cln_domain,
		     Link_side_id         const &cln_id,
		     Port_allocator_guard       *srv_port_alloc_ptr,
		     Domain                     &srv_domain,
		     Link_side_id         const &srv_id,
		     Cached_timer               &timer,
		     Configuration              &config,
		     L3_protocol          const  protocol,
		     Genode::Microseconds const  dissolve_timeout,
		     Interface_link_stats       &stats);

		~Link();

		void dissolve(bool timeout);

		void handle_config(Domain               &cln_domain,
		                   Domain               &srv_domain,
		                   Port_allocator_guard *srv_port_alloc_ptr,
		                   Configuration        &config);

		/*********
		 ** Log **
		 *********/

		void print(Genode::Output &output) const;


		/***************
		 ** Accessors **
		 ***************/

		Link_side       &client()           { return _client; }
		Link_side const &client()   const   { return _client; }
		Link_side       &server()           { return _server; }
		Link_side const &server()   const   { return _server; }
		Configuration   &config()           { return *_config_ptr; }
		L3_protocol      protocol() const   { return _protocol; }
		Interface       &client_interface() { return _client_interface; };
};


class Net::Tcp_link : public Link
{
	private:

		enum class State : Genode::uint8_t { OPENING, OPEN, CLOSING, CLOSED };

		struct Peer
		{
			bool syn       { false };
			bool syn_acked { false };
			bool fin       { false };
			bool fin_acked { false };
		};

		State _state  { State::OPENING };
		Peer  _client { };
		Peer  _server { };

		void _tcp_packet(Tcp_packet &tcp,
		                 Peer       &sender,
		                 Peer       &receiver);

		void _opening_tcp_packet(Tcp_packet const &tcp,
		                         Peer             &sender,
		                         Peer             &receiver);

		void _closing();

		void _closed();

	public:

		Tcp_link(Interface                 &cln_interface,
		         Domain                    &cln_domain,
		         Link_side_id        const &cln_id,
		         Port_allocator_guard      *srv_port_alloc_ptr,
		         Domain                    &srv_domain,
		         Link_side_id        const &srv_id,
		         Cached_timer              &timer,
		         Configuration             &config,
		         L3_protocol         const  protocol,
		         Interface_link_stats      &stats,
		         Tcp_packet                &tcp);

		void client_packet(Tcp_packet &tcp) { _tcp_packet(tcp, _client, _server); }

		void server_packet(Tcp_packet &tcp) { _tcp_packet(tcp, _server, _client); }

		bool can_early_drop() { return _state != State::OPEN; }
};


struct Net::Udp_link : Link
{
	Udp_link(Interface                     &cln_interface,
	         Domain                        &cln_domain,
	         Link_side_id            const &cln_id,
	         Port_allocator_guard          *srv_port_alloc_ptr,
	         Domain                        &srv_domain,
	         Link_side_id            const &srv_id,
	         Cached_timer                  &timer,
	         Configuration                 &config,
	         L3_protocol             const  protocol,
	         Interface_link_stats          &stats);

	void client_packet() { _packet(); }

	void server_packet();

	bool can_early_drop() { return true; }
};


struct Net::Icmp_link : Link
{
	Icmp_link(Interface                  &cln_interface,
	          Domain                     &cln_domain,
	          Link_side_id         const &cln_id,
	          Port_allocator_guard       *srv_port_alloc_ptr,
	          Domain                     &srv_domain,
	          Link_side_id         const &srv_id,
	          Cached_timer               &timer,
	          Configuration              &config,
	          L3_protocol          const  protocol,
	          Interface_link_stats       &stats);

	void client_packet() { _packet(); }

	void server_packet();

	bool can_early_drop() { return true; }
};

#endif /* _LINK_H_ */
