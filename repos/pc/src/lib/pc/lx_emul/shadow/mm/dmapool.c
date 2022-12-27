/*
 * \brief  Replaces mm/dmapool.c
 * \author josef soentgen
 * \date   2022-04-05
 */


/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */


#include <linux/slab.h>
#include <linux/dmapool.h>

struct dma_pool
{
	size_t size;
	size_t align;
};

void * dma_pool_alloc(struct dma_pool * pool, gfp_t mem_flags, dma_addr_t * handle)
{
	void * ret =
		lx_emul_mem_alloc_aligned_uncached(pool->size, pool->align);
	*handle = lx_emul_mem_dma_addr(ret);
	return ret;
}


struct dma_pool * dma_pool_create(const char * name,
                                  struct device * dev,
                                  size_t size,
                                  size_t align,
                                  size_t boundary)
{
	struct dma_pool * pool = kzalloc(sizeof(struct dma_pool), GFP_KERNEL);
	if (!pool)
		return NULL;

	/* TODO check if it makes sense to add min(align, PAGE_SIZE) check */

	pool->size  = size;
	pool->align = align;
	return pool;
}


struct dma_pool *dmam_pool_create(const char *name,
                                  struct device *dev,
                                  size_t size,
                                  size_t align,
                                  size_t allocation)
{
	/*
	 * Only take care of allocating the pool because
	 * we do not detach the driver anyway.
	 */
	return dma_pool_create(name, dev, size, align, 0);
}


void dma_pool_destroy(struct dma_pool * pool)
{
    kfree(pool);
}


void dma_pool_free(struct dma_pool * pool,void * vaddr,dma_addr_t dma)
{
    lx_emul_mem_free(vaddr);
}


