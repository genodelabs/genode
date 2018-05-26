/*
 * \brief  Rule for permitting ports in the context of a transport rule
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

/* local includes */
#include <permit_rule.h>
#include <domain.h>

using namespace Net;
using namespace Genode;


/*********************
 ** Permit_any_rule **
 *********************/

Domain &Permit_any_rule::_find_domain(Domain_tree    &domains,
                                      Xml_node const  node)
{
	try {
		return domains.find_by_name(
			node.attribute_value("domain", Domain_name()));
	}
	catch (Domain_tree::No_match) { throw Invalid(); }
}


void Permit_any_rule::print(Output &output) const
{
	Genode::print(output, "domain ", domain());
}


Permit_any_rule::Permit_any_rule(Domain_tree &domains, Xml_node const node)
:
	Permit_rule(_find_domain(domains, node))
{ }


/************************
 ** Permit_single_rule **
 ************************/

Domain &Permit_single_rule::_find_domain(Domain_tree    &domains,
                                         Xml_node const  node)
{
	try {
		return domains.find_by_name(
			node.attribute_value("domain", Domain_name()));
	}
	catch (Domain_tree::No_match) { throw Invalid(); }
}


bool Permit_single_rule::higher(Permit_single_rule *rule)
{
	return rule->_port.value > _port.value;
}


void Permit_single_rule::print(Output &output) const
{
	Genode::print(output, "port ", _port, " domain ", domain());
}


Permit_single_rule::Permit_single_rule(Domain_tree    &domains,
                                       Xml_node const  node)
:
	Permit_rule(_find_domain(domains, node)),
	_port(node.attribute_value("port", Port(0)))
{
	if (_port == Port(0) || dynamic_port(_port)) {
		throw Invalid(); }
}


Permit_single_rule const &
Permit_single_rule::find_by_port(Port const port) const
{
	if (port == _port) {
		return *this; }

	bool const side = port.value > _port.value;
	Permit_single_rule *const rule = Avl_node<Permit_single_rule>::child(side);
	if (!rule) {
		throw Permit_single_rule_tree::No_match(); }

	return rule->find_by_port(port);
}



/*****************************
 ** Permit_single_rule_tree **
 *****************************/

Permit_single_rule const &
Permit_single_rule_tree::find_by_port(Port const port) const
{
	Permit_single_rule *const rule = first();
	if (!rule) {
		throw No_match(); }

	return rule->find_by_port(port);
}
