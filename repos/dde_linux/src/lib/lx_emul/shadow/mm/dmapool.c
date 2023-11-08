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
	unsigned boundary; /* power-of-two */
	char name[32];
};

void * dma_pool_alloc(struct dma_pool * pool, gfp_t mem_flags, dma_addr_t * handle)
{
#ifdef CONFIG_X86
	void * ret =
		lx_emul_mem_alloc_aligned(pool->size, pool->align);
#else
	void * ret =
		lx_emul_mem_alloc_aligned_uncached(pool->size, pool->align);
#endif

	if (pool->boundary && ret) {
		unsigned long b = ((unsigned long)ret) >> pool->boundary;
		unsigned long e = ((unsigned long)ret + pool->size - 1) >> pool->boundary;

		if (b != e)
			printk("%s: allocation crosses %s pool boundary of %#lx bytes\n",
			       __func__, pool->name, 1ul << pool->boundary);
	}

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

	/* ensure allocations do not cross the given boundary */
	if (boundary)
		align = max_t(size_t, roundup_pow_of_two(size), align);

	pool->size     = size;
	pool->align    = align;
	pool->boundary = order_base_2(boundary);

	strscpy(pool->name, name, sizeof(pool->name));

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


/*
 * Caller guarantees that no more memory from the pool is in use,
 * and that nothing will try to use the pool after this call.
 */
void dma_pool_destroy(struct dma_pool * pool)
{
	kfree(pool);
}


void dma_pool_free(struct dma_pool * pool,void * vaddr,dma_addr_t dma)
{
	lx_emul_mem_free(vaddr);
}
