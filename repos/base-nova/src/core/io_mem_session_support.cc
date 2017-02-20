/*
 * \brief  Implementation of the IO_MEM session interface
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2009-03-29
 *
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <io_mem_session_component.h>
#include <platform.h>
#include <nova_util.h>

using namespace Genode;

void Io_mem_session_component::_unmap_local(addr_t base, size_t size) { }

addr_t Io_mem_session_component::_map_local(addr_t base, size_t size) {
	return 0; }
