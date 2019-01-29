/**
 * \brief  GDB debugging support
 * \author Sebastian Sumpf
 * \date   2015-03-12
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <debug.h>
#include <linker.h>

/*
 * GDB can set a breakpoint at this function to find out when ldso has loaded
 * the binary into memory.
 */
void binary_ready_hook_for_gdb() { }

/**
 * C-break function for GDB
 */
extern "C" void brk(Linker::Debug *, Linker::Link_map *) { }


void Linker::dump_link_map(Object const &obj)
{
	if (!obj.is_binary())
		log("  ",   Hex(obj.link_map().addr),
		    " .. ", Hex(obj.link_map().addr + obj.size() - 1),
		    ": ", obj.name());
}
