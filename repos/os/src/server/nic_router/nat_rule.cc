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


bool Nat_rule::higher(Nat_rule *rule)
{
	return (addr_t)&rule->domain() > (addr_t)&_domain;
}


Nat_rule::Nat_rule(Domain_dict    &domains,
                   Port_allocator &tcp_port_alloc,
                   Port_allocator &udp_port_alloc,
                   Port_allocator &icmp_port_alloc,
                   Xml_node const  node,
                   bool     const  verbose)
:
	_domain          { domains.deprecated_find_by_domain_attr<Invalid>(node) },
	_tcp_port_alloc  { tcp_port_alloc,  node.attribute_value("tcp-ports", 0U), verbose },
	_udp_port_alloc  { udp_port_alloc,  node.attribute_value("udp-ports", 0U), verbose },
	_icmp_port_alloc { icmp_port_alloc, node.attribute_value("icmp-ids",  0U), verbose }
{ }


void Nat_rule::print(Output &output) const
{
	Genode::print(output, "domain ", _domain,
	                  " tcp-ports ", _tcp_port_alloc.max_nr_of_ports(),
	                  " udp-ports ", _udp_port_alloc.max_nr_of_ports(),
	                   " icmp-ids ", _icmp_port_alloc.max_nr_of_ports());
}


Port_allocator_guard &Nat_rule::port_alloc(L3_protocol const prot)
{
	switch (prot) {
	case L3_protocol::TCP:  return _tcp_port_alloc;
	case L3_protocol::UDP:  return _udp_port_alloc;
	case L3_protocol::ICMP: return _icmp_port_alloc;
	default: throw Interface::Bad_transport_protocol(); }
}
