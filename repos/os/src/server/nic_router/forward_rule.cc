/*
 * \brief  Rule for forwarding a TCP/UDP port of the router to an interface
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
#include <forward_rule.h>
#include <domain.h>

/* Genode includes */
#include <util/xml_node.h>

using namespace Net;
using namespace Genode;


/******************
 ** Forward_rule **
 ******************/


Domain &Forward_rule::_find_domain(Domain_tree    &domains,
                                   Xml_node const  node)
{
	try {
		Domain_name const domain_name = node.attribute_value("domain", Domain_name());
		return domains.find_by_name(domain_name);
	}
	catch (Domain_tree::No_match) { throw Invalid(); }
}


void Forward_rule::print(Output &output) const
{
	Genode::print(output, "port ", _port, " domain ", _domain, " to ip ",
	              _to_ip, " to port ", _to_port);
}


Forward_rule::Forward_rule(Domain_tree &domains, Xml_node const node)
:
	_port    { node.attribute_value("port", Port(0)) },
	_to_ip   { node.attribute_value("to", Ipv4_address()) },
	_to_port { node.attribute_value("to_port", Port(0)) },
	_domain  { _find_domain(domains, node) }
{
	if (_port == Port(0) || !_to_ip.valid() || dynamic_port(_port)) {
		throw Invalid(); }
}


Forward_rule const &Forward_rule::find_by_port(Port const port) const
{
	if (port == _port) {
		return *this; }

	Forward_rule *const rule =
		Avl_node<Forward_rule>::child(port.value > _port.value);

	if (!rule) {
		throw Forward_rule_tree::No_match(); }

	return rule->find_by_port(port);
}


/***********************
 ** Forward_rule_tree **
 ***********************/

Forward_rule const &Forward_rule_tree::find_by_port(Port const port) const
{
	if (!first()) {
		throw No_match(); }

	return first()->find_by_port(port);
}
