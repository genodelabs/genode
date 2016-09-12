/*
 * \brief  Allocator for UDP/TCP ports
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
#include <port_allocator.h>

using namespace Net;
using namespace Genode;


Genode::uint16_t Port_allocator_guard::alloc()
{
	if (_used == _max) {
		throw Out_of_indices(); }

	uint16_t const port = _port_alloc.alloc();
	_used++;
	return port;
}


void Port_allocator_guard::free(Genode::uint16_t port)
{
	_port_alloc.free(port);
	_used = _used ? _used - 1 : 0;
}


Port_allocator_guard::Port_allocator_guard(Port_allocator & port_alloc,
                                           unsigned const max)
:
	_port_alloc(port_alloc), _max(max)
{ }


bool Net::dynamic_port(uint16_t const port)
{
	return port >= Port_allocator::FIRST &&
	       port < (uint32_t)Port_allocator::FIRST + Port_allocator::COUNT;
}
