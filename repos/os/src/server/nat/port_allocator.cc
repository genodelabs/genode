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


Port_allocator::Port_allocator()
{
	try { alloc_index(0); }
	catch (Already_allocated) { throw Failed_to_reserve_port_0(); }
}
