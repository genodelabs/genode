/*
 * \brief  Supplement for emulation of kernel/dma/mapping.c
 * \author Josef Soentgen
 * \date   2022-04-05
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */


#include <lx_emul/alloc.h>

#include <linux/dma-mapping.h>

dma_addr_t dma_map_page_attrs(struct device * dev,
                              struct page * page,
                              size_t offset,
                              size_t size,
                              enum dma_data_direction dir,
                              unsigned long attrs)
{
	dma_addr_t    const dma_addr  = page_to_phys(page);
	unsigned long const virt_addr = (unsigned long)page_to_virt(page);

	lx_emul_mem_cache_clean_invalidate((void *)(virt_addr + offset), size);
	return dma_addr + offset;
}


void dma_unmap_page_attrs(struct device * dev,
                          dma_addr_t addr,
                          size_t size,
                          enum dma_data_direction dir,
                          unsigned long attrs)
{
	unsigned long const virt_addr = lx_emul_mem_virt_addr((void*)addr);

	if (!virt_addr)
		return;

	if (dir == DMA_FROM_DEVICE)
		lx_emul_mem_cache_invalidate((void *)virt_addr, size);
}


void dma_sync_single_for_cpu(struct device *dev, dma_addr_t addr,
                             size_t size, enum dma_data_direction dir)
{
	unsigned long const virt_addr = lx_emul_mem_virt_addr((void*)addr);

	if (!virt_addr)
		return;

	lx_emul_mem_cache_invalidate((void *)virt_addr, size);
}


void dma_sync_single_for_device(struct device *dev, dma_addr_t addr,
                                size_t size, enum dma_data_direction dir)
{
	unsigned long const virt_addr = lx_emul_mem_virt_addr((void*)addr);

	if (!virt_addr)
		return;

	lx_emul_mem_cache_clean_invalidate((void *)virt_addr, size);
}


int dma_supported(struct device * dev,u64 mask)
{
	lx_emul_trace(__func__);
	return 1;
}
