/*
 * \brief  Implementation of the IO_MEM session interface
 * \author Norman Feske
 * \date   2015-05-01
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <io_mem_session_component.h>
#include <untyped_memory.h>


using namespace Core;


void Io_mem_session_component::_unmap_local(addr_t, size_t size, addr_t phys)
{
	Untyped_memory::convert_to_untyped_frames(phys, size);
}


addr_t Io_mem_session_component::_map_local(addr_t phys, size_t size)
{
	size_t const num_pages = size >> get_page_size_log2();
	Untyped_memory::convert_to_page_frames(phys, num_pages);

	return 0;
}
