/*
 * \brief  Allocator for UDP/TCP ports
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PORT_ALLOCATOR_H_
#define _PORT_ALLOCATOR_H_

/* Genode includes */
#include <util/bit_allocator.h>

namespace Net { class Port_allocator; }

class Net::Port_allocator
{
	public:

		enum { FIRST = 49152, COUNT = 16384 };

	private:

		Genode::Bit_allocator<COUNT> _alloc;

	public:

		Genode::uint16_t alloc() { return _alloc.alloc() + FIRST; }

		void free(Genode::uint16_t port) { _alloc.free(port - FIRST); }
};

#endif /* _PORT_ALLOCATOR_H_ */
