/*
 * \brief  Rule for doing NAT from one given interface to another
 * \author Martin Stein
 * \date   2016-09-13
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/xml_node.h>
#include <base/log.h>
#include <net/tcp.h>
#include <net/udp.h>

/* local includes */
#include <nat_rule.h>
#include <domain.h>
#include <interface.h>

using namespace Net;
using namespace Genode;


Domain &Nat_rule::_find_domain(Domain_tree    &domains,
                               Xml_node const  node)
{
	try {
		return domains.find_by_name(
			node.attribute_value("domain", Domain_name()));
	}
	catch (Domain_tree::No_match) { throw Invalid(); }
}


bool Nat_rule::higher(Nat_rule *rule)
{
	return (addr_t)&rule->domain() > (addr_t)&_domain;
}


Nat_rule::Nat_rule(Domain_tree    &domains,
                   Port_allocator &tcp_port_alloc,
                   Port_allocator &udp_port_alloc,
                   Port_allocator &icmp_port_alloc,
                   Xml_node const  node)
:
	_domain(_find_domain(domains, node)),
	_tcp_port_alloc (tcp_port_alloc,  node.attribute_value("tcp-ports", 0UL)),
	_udp_port_alloc (udp_port_alloc,  node.attribute_value("udp-ports", 0UL)),
	_icmp_port_alloc(icmp_port_alloc, node.attribute_value("icmp-ids", 0UL))
{ }


Nat_rule &Nat_rule::find_by_domain(Domain &domain)
{
	if (&domain == &_domain) {
		return *this; }

	bool const side = (addr_t)&domain > (addr_t)&_domain;
	Nat_rule *const rule = Avl_node<Nat_rule>::child(side);
	if (!rule) {
		throw Nat_rule_tree::No_match(); }

	return rule->find_by_domain(domain);
}


Nat_rule &Nat_rule_tree::find_by_domain(Domain &domain)
{
	Nat_rule *const rule = first();
	if (!rule) {
		throw No_match(); }

	return rule->find_by_domain(domain);
}


void Nat_rule::print(Output &output) const
{
	Genode::print(output, "domain ", _domain,
	                  " tcp-ports ", _tcp_port_alloc.max(),
	                  " udp-ports ", _udp_port_alloc.max(),
	                   " icmp-ids ", _icmp_port_alloc.max());
}


Port_allocator_guard &Nat_rule::port_alloc(L3_protocol const prot)
{
	switch (prot) {
	case L3_protocol::TCP:  return _tcp_port_alloc;
	case L3_protocol::UDP:  return _udp_port_alloc;
	case L3_protocol::ICMP: return _icmp_port_alloc;
	default: throw Interface::Bad_transport_protocol(); }
}
