/*
 * \brief  Replaces kernel/dma/mapping.c
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul/alloc.h>
#include <lx_emul/debug.h>

#include <linux/dma-mapping.h>
#include <linux/version.h>

void * dma_alloc_attrs(struct device * dev,
                       size_t          size,
                       dma_addr_t    * dma_handle,
                       gfp_t           flag,
                       unsigned long   attrs)
{
	void * addr;

#ifdef CONFIG_ARM
	if (dev && dev->dma_mem) {
		printk("We do not support device DMA memory yet!\n");
		lx_emul_trace_and_stop(__func__);
	}
#endif

#ifdef CONFIG_X86
	addr = lx_emul_mem_alloc_aligned(size, PAGE_SIZE);
#else
	addr = lx_emul_mem_alloc_aligned_uncached(size, PAGE_SIZE);
#endif
	*dma_handle = lx_emul_mem_dma_addr(addr);
	return addr;
}


void dma_free_attrs(struct device * dev,
                    size_t          size,
                    void          * cpu_addr,
                    dma_addr_t      dma_handle,
                    unsigned long   attrs)
{
	lx_emul_mem_free(cpu_addr);
}


int dma_set_mask(struct device *dev, u64 mask)
{
	mask = (dma_addr_t)mask;

	if (!dev->dma_mask)
		return -EIO;

	*dev->dma_mask = mask;
	return 0;
}


int dma_set_coherent_mask(struct device *dev, u64 mask)
{
	mask = (dma_addr_t)mask;

	dev->coherent_dma_mask = mask;
	return 0;
}


#if LINUX_VERSION_CODE < KERNEL_VERSION(5,15,0)
int          dma_map_sg_attrs(struct device          *dev,
#else
unsigned int dma_map_sg_attrs(struct device          *dev,
#endif
                              struct scatterlist     *sgl,
                              int                     nents,
                              enum dma_data_direction dir,
                              unsigned long           attrs)
{
	int i;
	struct scatterlist *sg;

	for_each_sg(sgl, sg, nents, i) {
		sg->dma_address = lx_emul_mem_dma_addr(page_address(sg_page(sg))) + sg->offset;
		if (!sg->dma_address)
			return 0;
		sg_dma_len(sg) = sg->length;
		lx_emul_mem_cache_clean_invalidate(page_address(sg_page(sg))
		                                   + sg->offset, sg->length);
	}

	return nents;
}


void dma_unmap_sg_attrs(struct device         * dev,
                        struct scatterlist    * sgl,
                        int                     nents,
                        enum dma_data_direction dir,
                        unsigned long           attrs)
{
	int i;
	struct scatterlist *sg;
	unsigned long virt_addr;

	if (dir != DMA_FROM_DEVICE)
		return;

	for_each_sg(sgl, sg, nents, i) {
		/*
		 * Since calling 'dma_unmap_sg_attrs' should be inverse to
		 * 'dma_map_sg_attrs' we use the formerly acquired 'dma_address'
		 * to look up the corresponding virtual address to invalidate the
		 * the cache.
		 */
		if (!sg->dma_address)
			continue;

		virt_addr =
			lx_emul_mem_virt_addr((void*)(sg->dma_address - sg->offset));
		lx_emul_mem_cache_invalidate((void*)(virt_addr + sg->offset),
		                             sg->length);
		sg->dma_address = 0;
	}
}


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
	/* do we need to evaluate the mask? */
	return 1;
}
