/*
 * \brief  Implementation of the IO_MEM session interface
 * \author Norman Feske
 * \date   2015-05-01
 */

/*
 * Copyright (C) 2015-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <io_mem_session_component.h>
#include <untyped_memory.h>


using namespace Core;


Io_mem_session_component::Dataspace_attr Io_mem_session_component::_acquire(Phys_range request)
{
	if (!request.req_size)
		return Dataspace_attr();

	auto const size = request.size();
	auto const base = request.base();

	size_t const num_pages = size >> get_page_size_log2();

	if (!Untyped_memory::convert_to_page_frames(base, num_pages))
		return Dataspace_attr();

	return Dataspace_attr(size, 0 /* no core local mapping */,
                          base, _cacheable, request.req_base);
}


void Io_mem_session_component::_release(Dataspace_attr const &attr)
{
	Untyped_memory::convert_to_untyped_frames(attr.phys_addr, attr.size);
}
