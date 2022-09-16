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

void Forward_rule::print(Output &output) const
{
	Genode::print(output, "port ", _port, " domain ", _domain, " to ip ",
	              _to_ip, " to port ", _to_port);
}


Forward_rule::Forward_rule(Domain_dict &domains, Xml_node const node)
:
	_port    { node.attribute_value("port", Port(0)) },
	_to_ip   { node.attribute_value("to", Ipv4_address()) },
	_to_port { node.attribute_value("to_port", Port(0)) },
	_domain  { domains.deprecated_find_by_domain_attr<Invalid>(node) }
{
	if (_port == Port(0) || !_to_ip.valid() || dynamic_port(_port)) {
		throw Invalid(); }
}
