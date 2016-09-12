/*
 * \brief  Rule for forwarding a TCP/UDP port of the router to an interface
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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

void Forward_rule::print(Output &output) const
{
	Genode::print(output, "port ", _port, " requests to ", _to,
	              " at ", _domain);
}


Forward_rule::Forward_rule(Domain_tree &domains, Xml_node const &node)
:
	Leaf_rule(domains, node),
	_port(node.attribute_value("port", 0UL)),
	_to(node.attribute_value("to", Ipv4_address()))
{
	if (!_port || !_to.valid() || dynamic_port(_port)) {
		throw Invalid(); }
}


Forward_rule const &Forward_rule::find_by_port(uint8_t const port) const
{
	if (port == _port) {
		return *this; }

	Forward_rule *const rule = Avl_node<Forward_rule>::child(port > _port);
	if (!rule) {
		throw Forward_rule_tree::No_match(); }

	return rule->find_by_port(port);
}


/***********************
 ** Forward_rule_tree **
 ***********************/

Forward_rule const &Forward_rule_tree::find_by_port(uint8_t const port) const
{
	if (!first()) {
		throw No_match(); }

	return first()->find_by_port(port);
}
