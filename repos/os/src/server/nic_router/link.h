/*
 * \brief  State tracking for UDP/TCP connections
 * \author Martin Stein
 * \date   2016-08-19
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
#include <pointer.h>

namespace Net {

	class  Configuration;
	class  Port_allocator_guard;
	class  Tcp_packet;
	class  Interface;
	class  Link_side_id;
	class  Link_side;
	class  Link_side_tree;
	class  Link;
	struct Link_list : Genode::List<Link> { };
	class  Tcp_link;
	class  Udp_link;
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

		Interface          &_interface;
		Link_side_id const  _id;
		Link               &_link;

	public:

		Link_side(Interface          &interface,
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

		Interface          &interface() const { return _interface; }
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

		using Signal_handler = Genode::Signal_handler<Link>;

		Configuration                       &_config;
		Link_side                            _client;
		Pointer<Port_allocator_guard> const  _server_port_alloc;
		Link_side                            _server;
		Timer::One_shot_timeout<Link>        _close_timeout;
		Genode::Microseconds          const  _close_timeout_us;
		Genode::uint8_t               const  _protocol;

		void _handle_close_timeout(Genode::Duration);

		void _packet() { _close_timeout.schedule(_close_timeout_us); }

	public:

		struct No_port_allocator : Genode::Exception { };

		Link(Interface                           &cln_interface,
		     Link_side_id                  const &cln_id,
		     Pointer<Port_allocator_guard> const  srv_port_alloc,
		     Interface                           &srv_interface,
		     Link_side_id                  const &srv_id,
		     Timer::Connection                   &timer,
		     Configuration                       &config,
		     Genode::uint8_t               const  protocol);

		void dissolve();


		/*********
		 ** Log **
		 *********/

		void print(Genode::Output &output) const;


		/***************
		 ** Accessors **
		 ***************/

		Link_side &client() { return _client; }
		Link_side &server() { return _server; }
};


class Net::Tcp_link : public Link
{
	private:

		bool _client_fin       = false;
		bool _server_fin       = false;
		bool _client_fin_acked = false;
		bool _server_fin_acked = false;
		bool _closed           = false;

		void _fin_acked();

	public:

		Tcp_link(Interface                           &cln_interface,
		         Link_side_id                  const &cln_id,
		         Pointer<Port_allocator_guard> const  srv_port_alloc,
		         Interface                           &srv_interface,
		         Link_side_id                  const &srv_id,
		         Timer::Connection                   &timer,
		         Configuration                       &config,
		         Genode::uint8_t               const  protocol);

		void client_packet(Tcp_packet &tcp);

		void server_packet(Tcp_packet &tcp);
};


struct Net::Udp_link : Link
{
	Udp_link(Interface                           &cln_interface,
	         Link_side_id                  const &cln_id,
	         Pointer<Port_allocator_guard> const  srv_port_alloc,
	         Interface                           &srv_interface,
	         Link_side_id                  const &srv_id,
	         Timer::Connection                   &timer,
	         Configuration                       &config,
	         Genode::uint8_t               const  protocol);

	void packet() { _packet(); }
};

#endif /* _LINK_H_ */
