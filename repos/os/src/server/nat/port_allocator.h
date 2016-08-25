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

#ifndef _PORT_ALLOCATOR_H_
#define _PORT_ALLOCATOR_H_

/* Genode includes */
#include <util/bit_allocator.h>

namespace Net {

	enum { NR_OF_PORTS = ((Genode::uint16_t)~0) + 1 };

	struct Port_allocator;
}

struct Net::Port_allocator : Genode::Bit_allocator<NR_OF_PORTS>
{
	struct Failed_to_reserve_port_0 : Genode::Exception { };

	Port_allocator();
};

#endif /* _PORT_ALLOCATOR_H_ */
