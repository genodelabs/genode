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
#include <util/arg_string.h>

/* core includes */
#include <io_port_session_component.h>

using namespace Core;


Io_port_session_component::Io_port_session_component(Range_allocator &io_port_alloc,
                                                     const char      *args)
:
	_io_port_range(io_port_alloc.alloc_addr(
		uint16_t(Arg_string::find_arg(args, "io_port_size").ulong_value(0)),
		uint16_t(Arg_string::find_arg(args, "io_port_base").ulong_value(0))))
{
	_io_port_range.with_error([&] (Alloc_error e) {
		switch (e) {
		case Alloc_error::DENIED:
			error("I/O-port range denied: ", args);
			return;

		case Alloc_error::OUT_OF_RAM:
			error("I/O port allocator ran out of RAM");
			return;

		case Alloc_error::OUT_OF_CAPS:
			error("I/O port allocator ran out of caps");
			return;
		}
	});
}
