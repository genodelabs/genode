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
#include <linux/version.h>
#include <../mm/slab.h>
#include <../mm/internal.h>
#include <lx_emul/alloc.h>
#include <lx_emul/debug.h>

void * krealloc(const void * p,size_t new_size,gfp_t flags)
{
	if (!p)
		return kmalloc(new_size, flags);

	if (!new_size) {
		kfree(p);
		return NULL;

	} else {

		unsigned long const old_size = ksize(p);
		void *ret;

		if (new_size <= old_size)
			return (void*) p;

		ret = kmalloc(new_size, flags);
		memcpy(ret, p, old_size);
		return ret;
	}
}


size_t ksize(const void * objp)
{
	if (objp == NULL)
		return 0;

	return __ksize(objp);
}


/*
 * We can use our __kmalloc() implementation here as it supports large
 * allocations well.
 */
void * kmalloc_order(size_t size, gfp_t flags, unsigned int order)
{
	return __kmalloc(size, flags);
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0) || defined(CONFIG_NUMA)
void * __kmalloc_node(size_t size, gfp_t flags, int node)
{
	return __kmalloc(size, flags);
}
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0)
void * kmalloc_large(size_t size,gfp_t flags)
{
	return __kmalloc(size, flags);
}
#endif


struct kmem_cache * kmem_cache_create(const char * name,
                                      unsigned int size,
                                      unsigned int align,
                                      slab_flags_t flags,
                                      void (* ctor)(void *))
{
	struct kmem_cache * cache =
		__kmalloc(sizeof(struct kmem_cache), GFP_KERNEL);
	cache->size     = size;
	cache->align    = align;
	cache->refcount = 1;
	return cache;
}


void kmem_cache_destroy(struct kmem_cache *cache)
{
	if (!cache)
		return;

	if (!cache->refcount) {
		printk("%s unexpected case - potential memory leak\n", __func__);
		return;
	}

	if (cache->refcount > 0)
		cache->refcount --;

	if (!cache->refcount)
		kfree(cache);
}
