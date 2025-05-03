/*
 * \brief  Linux-specific IO_PORT service
 * \author Johannes Kliemann
 * \date   2019-11-25
 */

/*
 * Copyright (C) 2006-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <io_port_session_component.h>

using namespace Core;


Io_port_session_component::Io_port_session_component(Range_allocator &, char const *)
:
	_io_port_range(Alloc_error::DENIED)
{ }
