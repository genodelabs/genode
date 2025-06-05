/*
 * \brief  Core implementation of the IO_MEM session interface
 * \author Christian Helmuth
 * \date   2006-08-01
 */

/*
 * Copyright (C) 2006-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/arg_string.h>
#include <root/root.h>

/* core includes */
#include <util.h>
#include <dataspace_component.h>
#include <io_mem_session_component.h>

using namespace Core;


Io_mem_session_component::Io_mem_session_component(Range_allocator &io_mem_alloc,
                                                   Range_allocator &ram_alloc,
                                                   Rpc_entrypoint  &ds_ep,
                                                   const char      *args)
:
	_io_mem_alloc(io_mem_alloc),
	_cacheable(_cacheable_attr(args)),
	_phys_attr(_phys_range(ram_alloc, args)),
	_ds_attr(_acquire(_phys_attr)),
	_io_mem_result(_io_mem_alloc.alloc_addr(_phys_attr.req_size,
	                                        _phys_attr.req_base)),
	_ds_ep(ds_ep)
{
	if (!_phys_attr.req_size || !_ds_attr.size || _io_mem_result.failed() || !_ds.valid()) {
		error("unable to access MMIO mapping: ", args);
		return;
	}

	_ds_cap = static_cap_cast<Io_mem_dataspace>(_ds_ep.manage(&_ds));
}


Io_mem_session_component::~Io_mem_session_component()
{
	/* remove all users of the to be destroyed io mem dataspace */
	_ds.detach_from_rm_sessions();

	/* dissolve IO_MEM dataspace from service entry point */
	if (_ds.cap().valid())
		_ds_ep.dissolve(&_ds);

	/*
	 * The Dataspace will remove itself from all RM sessions when its
	 * destructor is called. Thereby, it will get unmapped from all RM
	 * clients that currently have the dataspace attached.
	 */
}
