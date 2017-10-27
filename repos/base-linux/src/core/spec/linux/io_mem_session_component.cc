/*
 * \brief  Linux-specific IO_MEM service
 * \author Johannes Kliemann
 * \date   2019-11-25
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
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

size_t Io_mem_session_component::get_arg_size(const char *)
{
    warning(__func__, " not implemented");
    return 0;
}


addr_t Io_mem_session_component::get_arg_phys(const char *)
{
    warning(__func__, " not implemented");
    return 0;
}

Io_mem_session_component::Io_mem_session_component(Range_allocator &io_mem_alloc,
                                                   Range_allocator &,
                                                   Rpc_entrypoint  &ds_ep,
                                                   const char      *args) :
    _io_mem_alloc(io_mem_alloc),
    _ds(0, 0, 0, UNCACHED, true, 0),
    _ds_ep(ds_ep),
    _ds_cap(Io_mem_dataspace_capability())
{
	warning("no io_mem support on Linux (args=\"", args, "\")"); }

Cache_attribute Io_mem_session_component::get_arg_wc(const char *)
{
    warning(__func__, " not implemented");
    return UNCACHED;
}

Io_mem_dataspace_capability Io_mem_session_component::dataspace()
{
    return Io_mem_dataspace_capability();
}
