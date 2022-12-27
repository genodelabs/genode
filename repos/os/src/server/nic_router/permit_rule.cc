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

void Permit_any_rule::print(Output &output) const
{
	Genode::print(output, "domain ", domain());
}


Permit_any_rule::Permit_any_rule(Domain_dict &domains, Xml_node const node)
:
	Permit_rule { domains.deprecated_find_by_domain_attr<Invalid>(node) }
{ }


/************************
 ** Permit_single_rule **
 ************************/


bool Permit_single_rule::higher(Permit_single_rule *rule)
{
	return rule->_port.value > _port.value;
}


void Permit_single_rule::print(Output &output) const
{
	Genode::print(output, "port ", _port, " domain ", domain());
}


Permit_single_rule::Permit_single_rule(Domain_dict    &domains,
                                       Xml_node const  node)
:
	Permit_rule { domains.deprecated_find_by_domain_attr<Invalid>(node) },
	_port       { node.attribute_value("port", Port(0)) }
{
	if (_port == Port(0) || dynamic_port(_port)) {
		throw Invalid(); }
}
