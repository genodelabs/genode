/*
 * \brief  Implementation of the IO_MEM session interface
 * \author Martin Stein
 * \date   2012-02-12
 *
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <kernel/log.h>

/* core includes */
#include <io_mem_session_component.h>

using namespace Genode;


void Io_mem_session_component::_unmap_local(addr_t base, size_t size) { }


addr_t Io_mem_session_component::_map_local(addr_t base, size_t size)
{ return base; }

