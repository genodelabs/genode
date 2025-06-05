/*
 * \brief  Implementation of the IO_MEM session interface
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2009-03-29
 *
 */

/*
 * Copyright (C) 2009-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <io_mem_session_component.h>
#include <platform.h>
#include <nova_util.h>

using namespace Core;


Io_mem_session_component::Dataspace_attr Io_mem_session_component::_acquire(Phys_range request)
{
	if (!request.req_size)
		return Dataspace_attr();

	return Dataspace_attr(request.size(), 0 /* no core local mapping */,
	                      request.base(), _cacheable, request.req_base);
}


void Io_mem_session_component::_release(Dataspace_attr const &) { }
