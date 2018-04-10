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
#include <list.h>
#include <pointer.h>

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
	using Dhcp_allocation_list = List<Dhcp_allocation>;

	/* forward declarations */
	class Interface;
	class Domain;
	class Domain_tree;
}


class Net::Dhcp_server : private Genode::Noncopyable
{
	private:

		Ipv4_address         const    _dns_server;
		Pointer<Domain>      const    _dns_server_from;
		Genode::Microseconds const    _ip_lease_time;
		Ipv4_address         const    _ip_first;
		Ipv4_address         const    _ip_last;
		Genode::uint32_t     const    _ip_first_raw;
		Genode::uint32_t     const    _ip_count;
		Genode::Bit_allocator_dynamic _ip_alloc;

		Genode::Microseconds _init_ip_lease_time(Genode::Xml_node const node);

		Pointer<Domain> _init_dns_server_from(Genode::Xml_node const  node,
		                                      Domain_tree            &domains);

	public:

		enum { DEFAULT_IP_LEASE_TIME_SEC = 3600 };

		struct Alloc_ip_failed : Genode::Exception { };
		struct Invalid         : Genode::Exception { };

		Dhcp_server(Genode::Xml_node    const  node,
		            Domain              const &domain,
		            Genode::Allocator         &alloc,
		            Ipv4_address_prefix const &interface,
		            Domain_tree               &domains);

		Ipv4_address alloc_ip();

		void alloc_ip(Ipv4_address const &ip);

		void free_ip(Ipv4_address const &ip);

		bool ready() const;


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;


		/***************
		 ** Accessors **
		 ***************/

		Ipv4_address   const &dns_server()    const;
		Domain               &dns_server_from()     { return _dns_server_from(); }
		Genode::Microseconds  ip_lease_time() const { return _ip_lease_time; }
};


class Net::Dhcp_allocation : public  Genode::Avl_node<Dhcp_allocation>,
                             public  Dhcp_allocation_list::Element
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


struct Net::Dhcp_allocation_tree
{
	private:

		Genode::Avl_tree<Dhcp_allocation> _tree { };
		Dhcp_allocation_list              _list { };

	public:

		struct No_match : Genode::Exception { };

		Dhcp_allocation &find_by_mac(Mac_address const &mac) const;

		void insert(Dhcp_allocation &dhcp_alloc)
		{
			_tree.insert(&dhcp_alloc);
			_list.insert(&dhcp_alloc);
		}

		void remove(Dhcp_allocation &dhcp_alloc)
		{
			_tree.remove(&dhcp_alloc);
			_list.remove(&dhcp_alloc);
		}

		Dhcp_allocation *first() { return _tree.first(); }

		template <typename FUNC>
		void for_each(FUNC && functor)
		{
			using List_item = Dhcp_allocation_list::Element;
			for (Dhcp_allocation *item = _list.first(); item; )
			{
				Dhcp_allocation *const next_item = item->List_item::next();
				functor(*item);
				item = next_item;
			}
		}
};

#endif /* _DHCP_SERVER_H_ */
