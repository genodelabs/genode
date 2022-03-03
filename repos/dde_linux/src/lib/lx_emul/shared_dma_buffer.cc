/*
 * \brief  Lx_emul backend for shared dma buffers
 * \author Stefan Kalkowski
 * \date   2022-03-02
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/attached_dataspace.h>

#include <lx_kit/env.h>
#include <lx_kit/memory.h>
#include <lx_emul/page_virt.h>
#include <lx_emul/shared_dma_buffer.h>

struct genode_shared_dataspace : Genode::Attached_dataspace {};


extern "C" struct genode_shared_dataspace *
lx_emul_shared_dma_buffer_allocate(unsigned long size)
{
	Genode::Attached_dataspace & ds =
		Lx_kit::env().memory.alloc_dataspace(size);

	/*
	 * We have to call virt_to_pages eagerly here,
	 * to get contingous page objects registered
	 */
	lx_emul_virt_to_pages(ds.local_addr<void>(), size >> 12);
	return static_cast<genode_shared_dataspace*>(&ds);
}


extern "C" void
lx_emul_shared_dma_buffer_free(struct genode_shared_dataspace * ds)
{
	lx_emul_forget_pages(ds->local_addr<void>(), ds->size());
	Lx_kit::env().memory.free_dataspace(ds->local_addr<void>());
}


Genode::addr_t
genode_shared_dataspace_local_address(struct genode_shared_dataspace * ds)
{
	return (Genode::addr_t)ds->local_addr<void>();
}


Genode::Dataspace_capability
genode_shared_dataspace_capability(struct genode_shared_dataspace * ds)
{
	return ds->cap();
}
