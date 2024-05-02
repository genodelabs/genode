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


Forward_rule::Forward_rule(Port port, Ipv4_address to_ip, Port to_port, Domain &domain)
:
	_port    { port },
	_to_ip   { to_ip },
	_to_port { to_port },
	_domain  { domain }
{ }
