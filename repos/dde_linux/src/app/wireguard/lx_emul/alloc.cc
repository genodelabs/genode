/*
 * \brief  This file shadows repos/dde_linux/src/lib/lx_emul/alloc.cc
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2021-03-22
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/log.h>
#include <cpu/cache.h>
#include <lx_emul/alloc.h>
#include <lx_emul/page_virt.h>
#include <lx_kit/env.h>

extern "C" void * lx_emul_mem_alloc_aligned(unsigned long size, unsigned long align)
{
	void * const ptr = Lx_kit::env().memory.alloc(size, align);
	return ptr;
};


extern "C" void * lx_emul_mem_alloc_aligned_uncached(unsigned long size,
                                                     unsigned long align)
{
	void * const ptr = Lx_kit::env().uncached_memory.alloc(size, align);
	return ptr;
};


extern "C" unsigned long lx_emul_mem_dma_addr(void * addr)
{
	return (unsigned long)addr;
}


extern "C" unsigned long lx_emul_mem_virt_addr(void * dma_addr)
{
	return (unsigned long)dma_addr;
}


extern "C" void lx_emul_mem_free(const void * ptr)
{
	if (!ptr)
		return;
	if (Lx_kit::env().memory.free(ptr))
		return;
	if (Lx_kit::env().uncached_memory.free(ptr))
		return;
	Genode::error(__func__, " called with invalid ptr ", ptr);
};


extern "C" unsigned long lx_emul_mem_size(const void * ptr)
{
	unsigned long ret = 0;
	if (!ptr)
		return ret;
	if ((ret = Lx_kit::env().memory.size(ptr)))
		return ret;
	if (!(ret = Lx_kit::env().uncached_memory.size(ptr)))
		Genode::error(__func__, " called with invalid ptr ", ptr);
	return ret;
};


extern "C" void lx_emul_mem_cache_clean_invalidate(const void * addr,
                                                   unsigned long size)
{
	Genode::cache_clean_invalidate_data((Genode::addr_t)addr, size);
}


extern "C" void lx_emul_mem_cache_invalidate(const void * addr,
                                             unsigned long size)
{
	Genode::cache_invalidate_data((Genode::addr_t)addr, size);
}
