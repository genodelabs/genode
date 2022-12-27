/*
 * \brief  Replaces mm/slub.c
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/mm.h>
#include <linux/slab.h>
#include <../mm/slab.h>
#include <lx_emul/alloc.h>
#include <lx_emul/debug.h>

/*
 * We do not use those caches now,
 * but it is referenced by some compilation unit
 */
struct kmem_cache *
kmalloc_caches[NR_KMALLOC_TYPES][KMALLOC_SHIFT_HIGH + 1] = { NULL };


void kfree(const void * x)
{
	lx_emul_mem_free(x);
}

void * __kmalloc(size_t size, gfp_t flags)
{
	/* Linux expects a non-NULL return value for size 0 */
	if (size == 0)
		size = 1;

	/* DMA memory is not implemented yet */
	if (flags & GFP_DMA)
		lx_emul_trace_and_stop(__func__);

	return lx_emul_mem_alloc_aligned(size, ARCH_KMALLOC_MINALIGN);
}


struct kmem_cache * kmem_cache_create(const char * name,
                                      unsigned int size,
                                      unsigned int align,
                                      slab_flags_t flags,
                                      void (* ctor)(void *))
{
	struct kmem_cache * cache =
		__kmalloc(sizeof(struct kmem_cache), GFP_KERNEL);
	cache->size  = size;
	cache->align = align;
	return cache;
}


void kmem_cache_free(struct kmem_cache * s, void * x)
{
	lx_emul_mem_free(x);
}


void * __kmalloc_track_caller(size_t        size,
                              gfp_t         gfpflags,
                              unsigned long caller)
{
	return __kmalloc(size, gfpflags);
}


void * __kmalloc_node_track_caller(size_t        size,
                                   gfp_t         gfpflags,
                                   int           node,
                                   unsigned long caller)
{
	return __kmalloc_track_caller(size, gfpflags, caller);
}


static inline unsigned int kmem_cache_array_size_per_idx(unsigned idx)
{
	switch (idx) {
	case 0:  return 0;
	case 1:  return 96;
	case 2:  return 192;
	default: return 1 << idx;
	};
}


void __init kmem_cache_init(void)
{
	unsigned i;

	for (i = 0; i <= KMALLOC_SHIFT_HIGH; i++) {
		unsigned int sz = kmem_cache_array_size_per_idx(i);
		kmalloc_caches[KMALLOC_NORMAL][i] =
			kmem_cache_create("", sz, sz, GFP_KERNEL, NULL);
	}
}


void * kmem_cache_alloc(struct kmem_cache * s, gfp_t flags)
{
	unsigned long align;

	if (!s)
		lx_emul_trace_and_stop(__func__);

	align = max(s->align, (unsigned int)ARCH_KMALLOC_MINALIGN);
	return lx_emul_mem_alloc_aligned(s->size, align);
}


size_t __ksize(const void * object)
{
	return lx_emul_mem_size(object);
}


#ifdef CONFIG_NUMA

void * __kmalloc_node(size_t size, gfp_t flags, int node)
{
	return __kmalloc(size, flags);
}


void * kmem_cache_alloc_node(struct kmem_cache * s, gfp_t gfpflags, int node)
{
	return kmem_cache_alloc(s, gfpflags);
}

#endif /* CONFIG_NUMA */


#ifdef CONFIG_TRACING

#ifdef CONFIG_NUMA

void * kmem_cache_alloc_node_trace(struct kmem_cache * s,
                                   gfp_t gfpflags,
                                   int node,
                                   size_t size)
{
	return kmem_cache_alloc(s, gfpflags);
}

#endif /* CONFIG_NUMA */

void * kmem_cache_alloc_trace(struct kmem_cache * s,
                              gfp_t               gfpflags,
                              size_t              size)
{
	return __kmalloc(size, gfpflags);
}

#endif /* CONFIG_TRACING */
