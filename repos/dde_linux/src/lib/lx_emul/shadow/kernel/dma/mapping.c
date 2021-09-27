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

void * dma_alloc_attrs(struct device * dev,
                       size_t          size,
                       dma_addr_t    * dma_handle,
                       gfp_t           flag,
                       unsigned long   attrs)
{
	void * addr;

	if (dev && dev->dma_mem) {
		printk("We do not support device DMA memory yet!\n");
		lx_emul_trace_and_stop(__func__);
	}

	addr = lx_emul_mem_alloc_aligned_uncached(size, PAGE_SIZE);
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

	if (!dev->dma_mask || !dma_supported(dev, mask))
		return -EIO;

	*dev->dma_mask = mask;
	return 0;
}


int dma_set_coherent_mask(struct device *dev, u64 mask)
{
	mask = (dma_addr_t)mask;

	if (!dma_supported(dev, mask))
		return -EIO;

	dev->coherent_dma_mask = mask;
	return 0;
}


int dma_map_sg_attrs(struct device         * dev,
                     struct scatterlist    * sgl,
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

	for_each_sg(sgl, sg, nents, i) {
		lx_emul_mem_cache_invalidate(page_address(sg_page(sg)) + sg->offset,
		                             sg->length);
	}
}
