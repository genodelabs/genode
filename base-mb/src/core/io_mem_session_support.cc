/*
 * \brief  Implementation of the IO_MEM session interface
 * \author Norman Feske
 * \author Martin Stein
 * \date   2010-09-09
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <io_mem_session_component.h>

using namespace Genode;


void Io_mem_session_component::_unmap_local(addr_t base, size_t size)
{ }


addr_t Io_mem_session_component::_map_local(addr_t base, size_t size)
{
	/* Core memory gets mapped 1:1 except of the context area */
	return base;
}
