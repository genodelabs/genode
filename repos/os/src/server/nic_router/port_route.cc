/*
 * \brief  Port routing entry
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
#include <port_route.h>

using namespace Net;
using namespace Genode;


Port_route::Port_route(uint16_t dst, char const *label, size_t label_size,
                       Ipv4_address via, Ipv4_address to)
:
	_dst(dst), _label(Genode::Cstring(label, label_size)), _via(via), _to(to)
{ }


void Port_route::print(Output &output) const
{
	Genode::print(output, "", _dst, " -> \"", _label, "\" to ", _to,
	              " via ", _via);
}


Port_route *Port_route::find_by_dst(uint16_t dst)
{
	if (dst == _dst) {
		return this; }

	bool const side = dst > _dst;
	Port_route *c = Avl_node<Port_route>::child(side);
	return c ? c->find_by_dst(dst) : 0;
}


Port_route *Port_route_tree::find_by_dst(uint16_t dst)
{
	Port_route *port = first();
	if (!port) {
		return port; }

	port = port->find_by_dst(dst);
	return port;
}
