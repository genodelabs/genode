/*
 * \brief  Core implementation of the IO_MEM session interface
 * \author Christian Helmuth
 * \date   2006-08-01
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
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


Io_mem_session_component::Dataspace_attr
Io_mem_session_component::_prepare_io_mem(const char      *args,
                                          Range_allocator &ram_alloc)
{
	addr_t req_base = Arg_string::find_arg(args, "base").ulong_value(0);
	size_t req_size = Arg_string::find_arg(args, "size").ulong_value(0);

	/* align base and size on page boundaries */
	addr_t end  = align_addr(req_base + req_size, get_page_size_log2());
	addr_t base = req_base & ~(get_page_size() - 1);
	size_t size = end - base;

	_cacheable = UNCACHED;

	Arg a = Arg_string::find_arg(args, "wc");
	if (a.valid() && a.bool_value(0))
		_cacheable = WRITE_COMBINED;

	/* check for RAM collision */
	if (ram_alloc.remove_range(base, size).failed()) {
		error("I/O memory ", Hex_range<addr_t>(base, size), " "
		      "used by RAM allocator");
		return Dataspace_attr();
	}

	/* allocate region */
	if (_io_mem_alloc.alloc_addr(req_size, req_base).failed()) {
		error("I/O memory ", Hex_range<addr_t>(req_base, req_size), " not available");
		return Dataspace_attr();
	}

	/* request local mapping */
	addr_t local_addr = _map_local(base, size);

	return Dataspace_attr(size, local_addr, base, _cacheable, req_base);
}


Io_mem_session_component::Io_mem_session_component(Range_allocator &io_mem_alloc,
                                                   Range_allocator &ram_alloc,
                                                   Rpc_entrypoint  &ds_ep,
                                                   const char      *args)
:
	_io_mem_alloc(io_mem_alloc),
	_ds(_prepare_io_mem(args, ram_alloc)),
	_ds_ep(ds_ep)
{
	if (!_ds.valid()) {
		error("Local MMIO mapping failed!");

		_ds_cap = Io_mem_dataspace_capability();
		throw Service_denied();
	}

	_ds_cap = static_cap_cast<Io_mem_dataspace>(_ds_ep.manage(&_ds));
}


Io_mem_session_component::~Io_mem_session_component()
{
	/* dissolve IO_MEM dataspace from service entry point */
	_ds_ep.dissolve(&_ds);

	/* flush local mapping of IO_MEM */
	_unmap_local(_ds.core_local_addr(), _ds.size(), _ds.phys_addr());

	/*
	 * The Dataspace will remove itself from all RM sessions when its
	 * destructor is called. Thereby, it will get unmapped from all RM
	 * clients that currently have the dataspace attached.
	 */

	/* free region in IO_MEM allocator */
	_io_mem_alloc.free(reinterpret_cast<void *>(_ds.req_base));
}
