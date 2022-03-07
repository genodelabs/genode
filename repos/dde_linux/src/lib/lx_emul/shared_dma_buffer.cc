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

#include <lx_kit/env.h>
#include <lx_kit/dma_buffer.h>
#include <lx_emul/page_virt.h>
#include <lx_emul/shared_dma_buffer.h>

struct genode_shared_dataspace : Lx_kit::Dma_buffer {};


extern "C" struct genode_shared_dataspace *
lx_emul_shared_dma_buffer_allocate(unsigned long size)
{
	Lx_kit::Mem_allocator::Buffer & b = Lx_kit::env().memory.alloc_buffer(size);

	/*
	 * We have to call virt_to_pages eagerly here,
	 * to get contingous page objects registered
	 */
	lx_emul_virt_to_pages((void*)b.virt_addr(), size >> 12);
	return static_cast<genode_shared_dataspace*>(&b);
}


extern "C" void
lx_emul_shared_dma_buffer_free(struct genode_shared_dataspace * ds)
{
	lx_emul_forget_pages((void*)ds->virt_addr(), ds->size());
	Lx_kit::env().memory.free_buffer((void*)ds->virt_addr());
}


Genode::addr_t
genode_shared_dataspace_local_address(struct genode_shared_dataspace * ds)
{
	return ds->virt_addr();
}


Genode::Dataspace_capability
genode_shared_dataspace_capability(struct genode_shared_dataspace * ds)
{
	return ds->cap();
}
