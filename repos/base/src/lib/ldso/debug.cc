/**
 * \brief  GDB debugging support
 * \author Sebastian Sumpf
 * \date   2015-03-12
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <debug.h>
#include <linker.h>

/**
 * C-break function for GDB
 */
extern "C" void brk(Linker::Debug *, Linker::Link_map *) { }


void Linker::dump_link_map(Object *o)
{
	for (; o; o = o->next_obj()) {

		if (o->is_binary())
			continue;

		Genode::printf("  " EFMT " .. " EFMT ": %s\n",
		                o->link_map()->addr, o->link_map()->addr + o->size() - 1,
		                o->name());
	}
}
