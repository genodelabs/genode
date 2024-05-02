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

/* Genode includes */
#include <base/log.h>

/* local includes */
#include <port_allocator.h>

using namespace Net;
using namespace Genode;


bool Net::dynamic_port(Port const port)
{
	return port.value >= Port_allocator::FIRST_PORT;
}


/********************
 ** Port_allocator **
 ********************/

Net::Port_allocator::Alloc_result Net::Port_allocator::alloc()
{
	for (unsigned nr_of_trials { 0 };
	     nr_of_trials < NR_OF_PORTS;
	     nr_of_trials++) {

		uint16_t const port_offset = _next_port_offset;
		_next_port_offset = (_next_port_offset + 1) % NR_OF_PORTS;
		try {
			_bit_allocator.alloc_addr(port_offset);
			return Port { (uint16_t)(port_offset + FIRST_PORT) };
		}
		catch (Bit_allocator<NR_OF_PORTS>::Range_conflict) { }
	}
	return Alloc_error();
}


bool Net::Port_allocator::alloc(Port const port)
{
	try {
		_bit_allocator.alloc_addr(port.value - FIRST_PORT);
		return true;
	}
	catch (Bit_allocator<NR_OF_PORTS>::Range_conflict) { }
	return false;
}


void Port_allocator::free(Port const port)
{
	_bit_allocator.free(port.value - FIRST_PORT);
}


/**************************
 ** Port_allocator_guard **
 **************************/

Port_allocator_guard::Alloc_result Port_allocator_guard::alloc()
{
	if (_used_nr_of_ports == _max_nr_of_ports) {
		return Alloc_error();
	}
	Alloc_result const result = _port_alloc.alloc();
	if (result.failed())
		return result;

	_used_nr_of_ports++;
	return result;
}


bool Port_allocator_guard::alloc(Port const port)
{
	if (_used_nr_of_ports == _max_nr_of_ports)
		return false;

	if (!_port_alloc.alloc(port))
		return false;

	_used_nr_of_ports++;
	return true;
}


void Port_allocator_guard::free(Port const port)
{
	_port_alloc.free(port);
	_used_nr_of_ports = _used_nr_of_ports ? _used_nr_of_ports - 1 : 0;
}


Port_allocator_guard::Port_allocator_guard(Port_allocator &port_alloc,
                                           unsigned const  max_nr_of_ports,
                                           bool     const  verbose)
:
	_port_alloc  { port_alloc },
	_max_nr_of_ports {
		min(max_nr_of_ports,
		    static_cast<uint16_t>(Port_allocator::NR_OF_PORTS)) }
{
	if (verbose &&
	    max_nr_of_ports > (Port_allocator::NR_OF_PORTS)) {

		warning("number of ports was truncated to capacity of allocator");
	}
}

