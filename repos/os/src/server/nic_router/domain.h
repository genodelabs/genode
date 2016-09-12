/*
 * \brief  Reflects an effective domain configuration node
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DOMAIN_H_
#define _DOMAIN_H_

/* local includes */
#include <forward_rule.h>
#include <transport_rule.h>
#include <nat_rule.h>
#include <ip_rule.h>
#include <port_allocator.h>
#include <pointer.h>

/* Genode includes */
#include <util/avl_string.h>
#include <util/xml_node.h>

namespace Genode { class Allocator; }

namespace Net {

	class Interface;
	class Configuration;
	class Domain_avl_member;
	class Domain_base;
	class Domain;
	class Domain_tree;
	using Domain_name = Genode::String<160>;
}


class Net::Domain_avl_member : public Genode::Avl_string_base
{
	private:

		Domain &_domain;

	public:

		Domain_avl_member(Domain_name const &name,
		                  Domain            &domain);


		/***************
		 ** Accessors **
		 ***************/

		Domain &domain() const { return _domain; }
};


class Net::Domain_base
{
	protected:

		Domain_name const _name;

		Domain_base(Genode::Xml_node const &node);
};


class Net::Domain : public Domain_base
{
	private:

		Domain_avl_member       _avl_member;
		Configuration          &_config;
		Genode::Xml_node        _node;
		Genode::Allocator      &_alloc;
		Ipv4_address_prefix     _interface_attr;
		Ipv4_address const      _gateway;
		bool         const      _gateway_valid;
		Ip_rule_list            _ip_rules;
		Forward_rule_tree       _tcp_forward_rules;
		Forward_rule_tree       _udp_forward_rules;
		Transport_rule_list     _tcp_rules;
		Transport_rule_list     _udp_rules;
		Port_allocator          _tcp_port_alloc;
		Port_allocator          _udp_port_alloc;
		Nat_rule_tree           _nat_rules;
		Pointer<Interface>      _interface;

		void _read_forward_rules(Genode::Cstring  const &protocol,
		                         Domain_tree            &domains,
		                         Genode::Xml_node const &node,
		                         char             const *type,
		                         Forward_rule_tree      &rules);

		void _read_transport_rules(Genode::Cstring  const &protocol,
		                           Domain_tree            &domains,
		                           Genode::Xml_node const &node,
		                           char             const *type,
		                           Transport_rule_list    &rules);

	public:

		struct Invalid     : Genode::Exception { };
		struct No_next_hop : Genode::Exception { };

		Domain(Configuration          &config,
		       Genode::Xml_node const &node,
		       Genode::Allocator      &alloc);

		void create_rules(Domain_tree &domains);

		Ipv4_address const &next_hop(Ipv4_address const &ip) const;


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;


		/***************
		 ** Accessors **
		 ***************/

		Domain_name const   &name()              { return _name; }
		Ip_rule_list        &ip_rules()          { return _ip_rules; }
		Forward_rule_tree   &tcp_forward_rules() { return _tcp_forward_rules; }
		Forward_rule_tree   &udp_forward_rules() { return _udp_forward_rules; }
		Transport_rule_list &tcp_rules()         { return _tcp_rules; }
		Transport_rule_list &udp_rules()         { return _udp_rules; }
		Nat_rule_tree       &nat_rules()         { return _nat_rules; }
		Ipv4_address_prefix &interface_attr()    { return _interface_attr; }
		Pointer<Interface>  &interface()         { return _interface; }
		Configuration       &config() const      { return _config; }
		Domain_avl_member   &avl_member()        { return _avl_member; }
};


struct Net::Domain_tree : Genode::Avl_tree<Genode::Avl_string_base>
{
	using Avl_tree = Genode::Avl_tree<Genode::Avl_string_base>;

	struct No_match : Genode::Exception { };

	static Domain &domain(Genode::Avl_string_base const &node);

	Domain &find_by_name(Domain_name name);

	template <typename FUNC>
	void for_each(FUNC && functor) const {
		Avl_tree::for_each([&] (Genode::Avl_string_base const &node) {
			functor(domain(node));
		});
	}

	void insert(Domain &domain) { Avl_tree::insert(&domain.avl_member()); }
};

#endif /* _DOMAIN_H_ */
