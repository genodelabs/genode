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
#include <bit_allocator_dynamic.h>
#include <list.h>
#include <dns.h>
#include <ipv4_config.h>
#include <cached_timer.h>

/* Genode includes */
#include <net/mac_address.h>
#include <util/noncopyable.h>
#include <util/xml_node.h>
#include <timer_session/connection.h>

namespace Net {

	class Configuration;
	class Dhcp_server_base;
	class Dhcp_server;
	class Dhcp_allocation;
	class Dhcp_allocation_tree;
	using Dhcp_allocation_list = List<Dhcp_allocation>;

	/* forward declarations */
	class Interface;
	class Domain;
	class Domain_dict;
}


class Net::Dhcp_server_base
{
	protected:

		Genode::Allocator &_alloc;
		Dns_server_list    _dns_servers     { };
		Dns_domain_name    _dns_domain_name { _alloc };

		[[nodiscard]] bool _invalid(Domain const &domain,
		                            char   const *reason);

	public:

		Dhcp_server_base(Genode::Allocator &alloc);

		[[nodiscard]] bool finish_construction(Genode::Xml_node const &node,
		                                       Domain           const &domain);

		~Dhcp_server_base();
};


class Net::Dhcp_server : private Genode::Noncopyable,
                         private Net::Dhcp_server_base
{
	private:

		Domain                       *_dns_config_from_ptr { };
		Genode::Microseconds const    _ip_lease_time;
		Ipv4_address         const    _ip_first;
		Ipv4_address         const    _ip_last;
		Genode::uint32_t     const    _ip_first_raw;
		Genode::uint32_t     const    _ip_count;
		Genode::Bit_allocator_dynamic _ip_alloc;

		Genode::Microseconds _init_ip_lease_time(Genode::Xml_node const node);

		Ipv4_config const &_resolve_dns_config_from() const;

		/*
		 * Noncopyable
		 */
		Dhcp_server(Dhcp_server const &);
		Dhcp_server &operator = (Dhcp_server const &);

	public:

		enum { DEFAULT_IP_LEASE_TIME_SEC = 3600 };

		struct Invalid : Genode::Exception { };

		Dhcp_server(Genode::Xml_node const  node,
		            Genode::Allocator      &alloc);

		[[nodiscard]] bool finish_construction(Genode::Xml_node    const  node,
		                                       Domain_dict               &domains,
		                                       Domain                    &domain,
		                                       Ipv4_address_prefix const &interface);

		struct Alloc_ip_error { };
		using Alloc_ip_result = Genode::Attempt<Ipv4_address, Alloc_ip_error>;

		[[nodiscard]] Alloc_ip_result alloc_ip();

		[[nodiscard]] bool alloc_ip(Ipv4_address const &ip);

		void free_ip(Ipv4_address const &ip);

		bool has_invalid_remote_dns_cfg() const;

		void for_each_dns_server_ip(auto const &functor) const
		{
			if (_dns_config_from_ptr) {

				_resolve_dns_config_from().for_each_dns_server(
					[&] (Dns_server const &dns_server) {
						functor(dns_server.ip());
					});

			} else {

				_dns_servers.for_each([&] (Dns_server const &dns_server) {
					functor(dns_server.ip());
				});
			}
		}

		bool dns_servers_empty() const;

		Dns_domain_name const &dns_domain_name() const
		{
			if (_dns_config_from_ptr) {
				return _resolve_dns_config_from().dns_domain_name();
			}
			return _dns_domain_name;
		}

		bool config_equal_to_that_of(Dhcp_server const &dhcp_server) const;

		void with_dns_config_from(auto const &fn) const
		{
			if (_dns_config_from_ptr)
				fn(*_dns_config_from_ptr);
		}


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;


		/***************
		 ** Accessors **
		 ***************/

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
		                Cached_timer         &timer,
		                Genode::Microseconds  lifetime);

		~Dhcp_allocation();

		void find_by_mac(Mac_address const &mac, auto const &match_fn, auto const &no_match_fn)
		{
			if (mac == _mac) {
				match_fn(*this);
				return;
			}
			Dhcp_allocation *allocation_ptr = child(_higher(mac));
			if (allocation_ptr)
				allocation_ptr->find_by_mac(mac, match_fn, no_match_fn);
			else
				no_match_fn();
		}

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

		void find_by_mac(Mac_address const &mac, auto const &match_fn, auto const &no_match_fn) const
		{
			if (_tree.first())
				_tree.first()->find_by_mac(mac, match_fn, no_match_fn);
			else
				no_match_fn();
		}

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

		void for_each(auto const &functor)
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
