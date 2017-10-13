/*
 * \brief  Reflects an effective domain configuration node
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <configuration.h>
#include <l3_protocol.h>

/* Genode includes */
#include <util/xml_node.h>
#include <base/allocator.h>
#include <base/log.h>

using namespace Net;
using namespace Genode;


/*****************
 ** Dhcp_server **
 *****************/

Dhcp_server::Dhcp_server(Xml_node            const  node,
                         Allocator                 &alloc,
                         Ipv4_address_prefix const &interface)
:
	_dns_server(node.attribute_value("dns_server", Ipv4_address())),
	_ip_lease_time(_init_ip_lease_time(node)),
	_ip_first(node.attribute_value("ip_first", Ipv4_address())),
	_ip_last(node.attribute_value("ip_last", Ipv4_address())),
	_ip_first_raw(_ip_first.to_uint32_little_endian()),
	_ip_count(_ip_last.to_uint32_little_endian() - _ip_first_raw),
	_ip_alloc(alloc, _ip_count)
{
	if (!interface.prefix_matches(_ip_first) ||
	    !interface.prefix_matches(_ip_last) ||
	    interface.address.is_in_range(_ip_first, _ip_last))
	{
		throw Invalid();
	}
}


Microseconds Dhcp_server::_init_ip_lease_time(Xml_node const node)
{
	unsigned long ip_lease_time_sec =
		node.attribute_value("ip_lease_time_sec", 0UL);

	if (!ip_lease_time_sec) {
		warning("fall back to default ip_lease_time_sec=\"",
		        (unsigned long)DEFAULT_IP_LEASE_TIME_SEC, "\"");
		ip_lease_time_sec = DEFAULT_IP_LEASE_TIME_SEC;
	}
	return Microseconds((unsigned long)ip_lease_time_sec * 1000 * 1000);
}


void Dhcp_server::print(Output &output) const
{
	if (_dns_server.valid()) {
		Genode::print(output, "DNS server ", _dns_server, " ");
	}
	Genode::print(output, "IP first ", _ip_first,
	                        ", last ", _ip_last,
	                       ", count ", _ip_count,
	                  ", lease time ", _ip_lease_time.value / 1000 / 1000, " sec");
}


Ipv4_address Dhcp_server::alloc_ip()
{
	try {
		return Ipv4_address::from_uint32_little_endian(_ip_alloc.alloc() +
		                                               _ip_first_raw);
	}
	catch (Bit_allocator_dynamic::Out_of_indices) {
		throw Alloc_ip_failed();
	}
}


void Dhcp_server::free_ip(Ipv4_address const &ip)
{
	_ip_alloc.free(ip.to_uint32_little_endian() - _ip_first_raw);
}


/***********************
 ** Domain_avl_member **
 ***********************/

Domain_avl_member::Domain_avl_member(Domain_name const &name,
                                     Domain            &domain)
:
	Avl_string_base(name.string()), _domain(domain)
{ }


/*****************
 ** Domain_base **
 *****************/

Domain_base::Domain_base(Xml_node const node)
: _name(node.attribute_value("name", Domain_name())) { }


/************
 ** Domain **
 ************/

void Domain::_read_forward_rules(Cstring  const    &protocol,
                                 Domain_tree       &domains,
                                 Xml_node const     node,
                                 char     const    *type,
                                 Forward_rule_tree &rules)
{
	node.for_each_sub_node(type, [&] (Xml_node const node) {
		try {
			Forward_rule &rule = *new (_alloc) Forward_rule(domains, node);
			rules.insert(&rule);
			if (_config.verbose()) {
				log("  Forward rule: ", protocol, " ", rule); }
		}
		catch (Rule::Invalid) { warning("invalid forward rule"); }
	});
}


void Domain::_read_transport_rules(Cstring  const      &protocol,
                                   Domain_tree         &domains,
                                   Xml_node const       node,
                                   char     const      *type,
                                   Transport_rule_list &rules)
{
	node.for_each_sub_node(type, [&] (Xml_node const node) {
		try {
			rules.insert(*new (_alloc) Transport_rule(domains, node, _alloc,
			                                          protocol, _config));
		}
		catch (Rule::Invalid) { warning("invalid transport rule"); }
	});
}


void Domain::print(Output &output) const
{
	Genode::print(output, _name);
}


Domain::Domain(Configuration &config, Xml_node const node, Allocator &alloc)
:
	Domain_base(node), _avl_member(_name, *this), _config(config),
	_node(node), _alloc(alloc),
	_ip_config(_node.attribute_value("interface", Ipv4_address_prefix()),
	           _node.attribute_value("gateway",   Ipv4_address()))
{
	if (_name == Domain_name() || !_ip_config.interface_valid) {
		throw Invalid();
	}
	/* try to find configuration for DHCP server role */
	try {
		_dhcp_server.set(*new (alloc)
			Dhcp_server(node.sub_node("dhcp-server"), alloc,
			            _ip_config.interface));

		if (_config.verbose()) {
			log("  DHCP server: ", _dhcp_server.deref()); }
	}
	catch (Xml_node::Nonexistent_sub_node) { }
	catch (Dhcp_server::Invalid) {
		error("Invalid DHCP server configuration at domain ", *this); }
}


Domain::~Domain()
{
	try { destroy(_alloc, &_dhcp_server.deref()); }
	catch (Pointer<Dhcp_server>::Invalid) { }
}


void Domain::create_rules(Domain_tree &domains)
{
	/* read forward rules */
	_read_forward_rules(tcp_name(), domains, _node, "tcp-forward",
	                    _tcp_forward_rules);
	_read_forward_rules(udp_name(), domains, _node, "udp-forward",
	                    _udp_forward_rules);

	/* read UDP and TCP rules */
	_read_transport_rules(tcp_name(), domains, _node, "tcp", _tcp_rules);
	_read_transport_rules(udp_name(), domains, _node, "udp", _udp_rules);

	/* read NAT rules */
	_node.for_each_sub_node("nat", [&] (Xml_node const node) {
		try {
			_nat_rules.insert(
				new (_alloc) Nat_rule(domains, _tcp_port_alloc,
				                      _udp_port_alloc, node));
		}
		catch (Rule::Invalid) { warning("invalid NAT rule"); }
	});
	/* read IP rules */
	_node.for_each_sub_node("ip", [&] (Xml_node const node) {
		try { _ip_rules.insert(*new (_alloc) Ip_rule(domains, node)); }
		catch (Rule::Invalid) { warning("invalid IP rule"); }
	});
}


Ipv4_address const &Domain::next_hop(Ipv4_address const &ip) const
{
	if (_ip_config.interface.prefix_matches(ip)) { return ip; }
	if (_ip_config.gateway_valid) { return _ip_config.gateway; }
	throw No_next_hop();
}


/*****************
 ** Domain_tree **
 *****************/

Domain &Domain_tree::domain(Avl_string_base const &node)
{
	return static_cast<Domain_avl_member const *>(&node)->domain();
}

Domain &Domain_tree::find_by_name(Domain_name name)
{
	if (name == Domain_name() || !first()) {
		throw No_match(); }

	Avl_string_base *node = first()->find_by_name(name.string());
	if (!node) {
		throw No_match(); }

	return domain(*node);
}
