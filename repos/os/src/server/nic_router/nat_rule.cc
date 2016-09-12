/*
 * \brief  Rule for doing NAT from one given interface to another
 * \author Martin Stein
 * \date   2016-09-13
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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


bool Nat_rule::higher(Nat_rule *rule)
{
	return (addr_t)&rule->domain() > (addr_t)&_domain;
}


Nat_rule::Nat_rule(Domain_tree    &domains,
                   Port_allocator &tcp_port_alloc,
                   Port_allocator &udp_port_alloc,
                   Xml_node const &node)
:
	Leaf_rule(domains, node),
	_tcp_port_alloc(tcp_port_alloc, node.attribute_value("tcp-ports", 0UL)),
	_udp_port_alloc(udp_port_alloc, node.attribute_value("udp-ports", 0UL))
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
	              " udp-ports ", _udp_port_alloc.max());
}


Port_allocator_guard &Nat_rule::port_alloc(uint8_t const prot)
{
	switch (prot) {
	case Tcp_packet::IP_ID: return _tcp_port_alloc;
	case Udp_packet::IP_ID: return _udp_port_alloc;
	default: throw Interface::Bad_transport_protocol(); }
}
