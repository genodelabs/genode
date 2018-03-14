/*
 * \brief  Allocator for UDP/TCP ports
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
#include <port_allocator.h>

using namespace Net;
using namespace Genode;


bool Net::dynamic_port(Port const port)
{
	return port.value >= (unsigned)Port_allocator::FIRST &&
	       port.value <  (unsigned)Port_allocator::FIRST +
	                               Port_allocator::COUNT;
}


/********************
 ** Port_allocator **
 ********************/

void Net::Port_allocator::alloc(Port const port)
{
	try { _alloc.alloc_addr(port.value - FIRST); }
	catch (Genode::Bit_allocator<COUNT>::Range_conflict) {
		throw Allocation_conflict(); }
}


/**************************
 ** Port_allocator_guard **
 **************************/

Port Port_allocator_guard::alloc()
{
	if (_used == _max) {
		throw Out_of_indices(); }

	Port const port = _port_alloc.alloc();
	_used++;
	return port;
}


void Port_allocator_guard::alloc(Port const port)
{
	if (_used == _max) {
		throw Out_of_indices(); }

	_port_alloc.alloc(port);
	_used++;
}


void Port_allocator_guard::free(Port const port)
{
	_port_alloc.free(port);
	_used = _used ? _used - 1 : 0;
}


Port_allocator_guard::Port_allocator_guard(Port_allocator &port_alloc,
                                           unsigned const  max)
:
	_port_alloc(port_alloc), _max(max)
{ }
