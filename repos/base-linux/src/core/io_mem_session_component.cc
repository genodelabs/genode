/*
 * \brief  Linux-specific IO_MEM service
 * \author Christian Helmuth
 * \date   2006-09-01
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>

#include <io_mem_session_component.h>

using namespace Genode;


Io_mem_session_component::Io_mem_session_component(Range_allocator *,
                                                   Range_allocator *,
                                                   Rpc_entrypoint  *,
                                                   const char      *args) {
	warning("no io_mem support on Linux (args=\"", args, "\")"); }
