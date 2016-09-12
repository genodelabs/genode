/*
 * \brief  Rule for permitting ports in the context of a transport rule
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>

/* local includes */
#include <permit_rule.h>
#include <domain.h>

using namespace Net;
using namespace Genode;


/*****************
 ** Permit_rule **
 *****************/

Permit_rule::Permit_rule(Domain_tree &domains, Xml_node const &node)
:
	Leaf_rule(domains, node)
{ }


/*********************
 ** Permit_any_rule **
 *********************/

void Permit_any_rule::print(Output &output) const
{
	Genode::print(output, "requests to ", _domain);
}


Permit_any_rule::Permit_any_rule(Domain_tree &domains, Xml_node const &node)
:
	Permit_rule(domains, node)
{ }


/************************
 ** Permit_single_rule **
 ************************/

bool Permit_single_rule::higher(Permit_single_rule *rule)
{
	return rule->_port > _port;
}


void Permit_single_rule::print(Output &output) const
{
	Genode::print(output, "port ", _port, " requests to ", _domain);
}


Permit_single_rule::Permit_single_rule(Domain_tree    &domains,
                                       Xml_node const &node)
:
	Permit_rule(domains, node),
	_port(node.attribute_value("port", 0UL))
{
	if (!_port || dynamic_port(_port)) {
		throw Invalid(); }
}


Permit_single_rule const &
Permit_single_rule::find_by_port(uint16_t const port) const
{
	if (port == _port) {
		return *this; }

	bool const side = port > _port;
	Permit_single_rule *const rule = Avl_node<Permit_single_rule>::child(side);
	if (!rule) {
		throw Permit_single_rule_tree::No_match(); }

	return rule->find_by_port(port);
}



/*****************************
 ** Permit_single_rule_tree **
 *****************************/

Permit_single_rule const &
Permit_single_rule_tree::find_by_port(uint16_t const port) const
{
	Permit_single_rule *const rule = first();
	if (!rule) {
		throw No_match(); }

	return rule->find_by_port(port);
}
