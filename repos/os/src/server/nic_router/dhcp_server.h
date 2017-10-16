/*
 * \brief  DHCP server role of a domain
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DHCP_SERVER_H_
#define _DHCP_SERVER_H_

/* local includes */
#include <ipv4_address_prefix.h>
#include <bit_allocator_dynamic.h>

/* Genode includes */
#include <net/mac_address.h>
#include <util/noncopyable.h>
#include <util/xml_node.h>
#include <timer_session/connection.h>

namespace Net {

	class Dhcp_server;
	class Dhcp_allocation;
	class Dhcp_allocation;
	class Dhcp_allocation_tree;
	using Dhcp_allocation_list = Genode::List<Dhcp_allocation>;

	/* forward declarations */
	class Interface;
}


class Net::Dhcp_server : private Genode::Noncopyable
{
	private:

		Ipv4_address         const    _dns_server;
		Genode::Microseconds const    _ip_lease_time;
		Ipv4_address         const    _ip_first;
		Ipv4_address         const    _ip_last;
		Genode::uint32_t     const    _ip_first_raw;
		Genode::uint32_t     const    _ip_count;
		Genode::Bit_allocator_dynamic _ip_alloc;

		Genode::Microseconds _init_ip_lease_time(Genode::Xml_node const node);

	public:

		enum { DEFAULT_IP_LEASE_TIME_SEC = 3600 };

		struct Alloc_ip_failed : Genode::Exception { };
		struct Invalid         : Genode::Exception { };

		Dhcp_server(Genode::Xml_node    const  node,
		            Genode::Allocator         &alloc,
		            Ipv4_address_prefix const &interface);

		Ipv4_address alloc_ip();

		void free_ip(Ipv4_address const &ip);


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;


		/***************
		 ** Accessors **
		 ***************/

		Ipv4_address   const &dns_server()    const { return _dns_server; }
		Genode::Microseconds  ip_lease_time() const { return _ip_lease_time; }
};


struct Net::Dhcp_allocation_tree : public  Genode::Avl_tree<Dhcp_allocation>,
                                   private Genode::Noncopyable
{
	struct No_match : Genode::Exception { };

	Dhcp_allocation &find_by_mac(Mac_address const &mac) const;
};


class Net::Dhcp_allocation : public  Genode::Avl_node<Dhcp_allocation>,
                             public  Dhcp_allocation_list::Element,
                             private Genode::Noncopyable
{
	protected:

		Interface                                &_interface;
		Ipv4_address                       const  _ip;
		Mac_address                        const  _mac;
		Timer::One_shot_timeout<Dhcp_allocation>  _timeout;
		bool                                      _bound { false };

		void _handle_timeout(Genode::Duration);

		bool _higher(Mac_address const &mac) const;

	public:

		Dhcp_allocation(Interface            &interface,
		                Ipv4_address   const &ip,
		                Mac_address    const &mac,
		                Timer::Connection    &timer,
		                Genode::Microseconds  lifetime);

		Dhcp_allocation &find_by_mac(Mac_address const &mac);

		void lifetime(Genode::Microseconds lifetime);


		/**************
		 ** Avl_node **
		 **************/

		bool higher(Dhcp_allocation *alloc) { return _higher(alloc->_mac); }


		/*********
		 ** Log **
		 *********/

		void print(Genode::Output &output) const;


		/***************
		 ** Accessors **
		 ***************/

		Ipv4_address const &ip()    const { return _ip; }
		bool                bound() const { return _bound; }

		void set_bound() { _bound = true; }
};

#endif /* _DHCP_SERVER_H_ */
