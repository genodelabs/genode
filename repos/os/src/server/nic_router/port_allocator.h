/*
 * \brief  Allocator for UDP/TCP ports
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PORT_ALLOCATOR_H_
#define _PORT_ALLOCATOR_H_

/* Genode includes */
#include <net/port.h>

/* local includes */
#include <util/bit_allocator.h>

namespace Net {

	class Port_allocator;
	class Port_allocator_guard;

	bool dynamic_port(Port const port);
}


class Net::Port_allocator
{
	public:

		enum { FIRST_PORT = 49152, NR_OF_PORTS = 16384 };

	private:

		Genode::Bit_allocator<NR_OF_PORTS> _bit_allocator    { };
		Genode::uint16_t                   _next_port_offset { 0 };

	public:

		struct Allocation_conflict : Genode::Exception { };
		struct Out_of_indices      : Genode::Exception { };

		Port alloc();

		void alloc(Port const port);

		void free(Port const port);
};


class Net::Port_allocator_guard
{
	private:

		Port_allocator &_port_alloc;
		unsigned const  _max_nr_of_ports;
		unsigned        _used_nr_of_ports { 0 };

	public:

		class Out_of_indices : Genode::Exception {};

		Port alloc();

		void alloc(Port const port);

		void free(Port const port);

		Port_allocator_guard(Port_allocator &port_alloc,
		                     unsigned const  max_nr_of_ports,
		                     bool     const  verbose);

		unsigned max_nr_of_ports() const { return _max_nr_of_ports; }
};

#endif /* _PORT_ALLOCATOR_H_ */
