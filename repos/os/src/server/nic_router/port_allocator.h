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
#include <util/bit_allocator.h>
#include <net/port.h>

namespace Net {

	class Port_allocator;
	class Port_allocator_guard;

	bool dynamic_port(Port const port);
}


class Net::Port_allocator
{
	public:

		enum { FIRST = 49152, COUNT = 16384 };

	private:

		Genode::Bit_allocator<COUNT> _alloc;

	public:

		Port alloc() { return Port(_alloc.alloc() + FIRST); }

		void free(Port const port) { _alloc.free(port.value - FIRST); }
};


class Net::Port_allocator_guard
{
	private:

		Port_allocator &_port_alloc;
		unsigned const  _max;
		unsigned        _used = 0;

	public:

		class Out_of_indices : Genode::Exception {};

		Port alloc();

		void free(Port const port);

		Port_allocator_guard(Port_allocator & port_alloc, unsigned const max);

		unsigned max() const { return _max; }
};

#endif /* _PORT_ALLOCATOR_H_ */
