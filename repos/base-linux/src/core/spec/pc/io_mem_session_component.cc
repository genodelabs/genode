/*
 * \brief  Linux-specific IO_MEM service
 * \author Johannes Kliemann
 * \date   2019-11-25
 */

/*
 * Copyright (C) 2006-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <linux_dataspace/client.h>
#include <base/internal/page_size.h>

#include <core_linux_syscalls.h>

#include <io_mem_session_component.h>


using namespace Genode;

size_t Io_mem_session_component::get_arg_size(const char *args)
{
	size_t const size = Arg_string::find_arg(args, "size").ulong_value(0);
	addr_t base = Arg_string::find_arg(args, "base").ulong_value(0);

	addr_t end = align_addr(base + size, get_page_size_log2());
	base = base & ~(get_page_size() - 1);
	return (size_t)(end - base);
}


addr_t Io_mem_session_component::get_arg_phys(const char *args)
{
	return Arg_string::find_arg(args, "base").ulong_value(0);
}

Cache_attribute Io_mem_session_component::get_arg_wc(const char *args)
{
	Arg a = Arg_string::find_arg("wc", args);
	if (a.valid() && a.bool_value(0)) {
		return WRITE_COMBINED;
	} else {
		return UNCACHED;
	}
}


Io_mem_session_component::Io_mem_session_component(Range_allocator &io_mem_alloc,
                                                   Range_allocator &,
                                                   Rpc_entrypoint  &ds_ep,
                                                   const char      *args) :
	_io_mem_alloc(io_mem_alloc),
	_ds(get_arg_size(args), 0, get_arg_phys(args), get_arg_wc(args), true, 0),
	_ds_ep(ds_ep),
	_ds_cap(Io_mem_dataspace_capability())
{
	int _fd = -1;

	_fd = lx_open("/dev/hwio", O_RDWR | O_SYNC);
	lx_ioctl_iomem(_fd, (unsigned long)get_arg_phys(args), get_arg_size(args));
	if (_fd > 0) {
		_ds.fd(_fd);
	} else {
		error("Failed to open /dev/hwio");
		throw Genode::Service_denied();
	}

	_ds_cap = static_cap_cast<Io_mem_dataspace>(static_cap_cast<Dataspace>(_ds_ep.manage(&_ds)));
}

Io_mem_dataspace_capability Io_mem_session_component::dataspace()
{
    return _ds_cap;
}
