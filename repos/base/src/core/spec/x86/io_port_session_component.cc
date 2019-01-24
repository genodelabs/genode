/*
 * \brief  Core implementation of the IO_PORT session interface
 * \author Christian Helmuth
 * \date   2007-04-17
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/string.h>
#include <util/arg_string.h>
#include <root/root.h>

/* core includes */
#include <io_port_session_component.h>

using namespace Genode;


/******************************
 ** Constructor / destructor **
 ******************************/

Io_port_session_component::Io_port_session_component(Range_allocator &io_port_alloc,
                                                     const char      *args)
: _io_port_alloc(io_port_alloc)
{
	/* parse for port properties */
	unsigned base = Arg_string::find_arg(args, "io_port_base").ulong_value(0);
	unsigned size = Arg_string::find_arg(args, "io_port_size").ulong_value(0);

	/* allocate region (also checks out-of-bounds regions) */
	switch (io_port_alloc.alloc_addr(size, base).value) {

	case Range_allocator::Alloc_return::RANGE_CONFLICT:
		error("I/O port ", Hex_range<uint16_t>(base, size), " not available");
		throw Service_denied();

	case Range_allocator::Alloc_return::OUT_OF_METADATA:
		error("I/O port allocator ran out of meta data");
		throw Service_denied();

	case Range_allocator::Alloc_return::OK: break;
	}

	/* store information */
	_base = base;
	_size = size;
}


Io_port_session_component::~Io_port_session_component()
{
	_io_port_alloc.free(reinterpret_cast<void *>(_base));
}
