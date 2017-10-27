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

#include <util/string.h>
#include <util/arg_string.h>
#include <root/root.h>
#include <io_port_session_component.h>


Genode::Io_port_session_component::Io_port_session_component(Genode::Range_allocator &io_port_alloc, const char *)
: _io_port_alloc(io_port_alloc)
{
    Genode::warning("IO PORTS are not supported on base linux");
}

Genode::Io_port_session_component::~Io_port_session_component()
{}
