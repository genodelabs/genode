/*
 * \brief  Implementation of linux/slab.h
 * \author Norman Feske
 * \date   2015-09-09
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Linux kit includes */
#include <legacy/lx_kit/malloc.h>


void *kmalloc(size_t size, gfp_t flags)
{
	if (flags & __GFP_DMA)
		Genode::warning("GFP_DMA memory (below 16 MiB) requested "
		                "(", __builtin_return_address(0), ")");
	if (flags & __GFP_DMA32)
		Genode::warning("GFP_DMA32 memory (below 4 GiB) requested"
		                "(", __builtin_return_address(0), ")");

	void *addr = nullptr;

	addr = (flags & GFP_LX_DMA)
		? Lx::Malloc::dma().alloc(size)
		: Lx::Malloc::mem().alloc(size);

	if (!addr)
		return 0;

	if ((Genode::addr_t)addr & 0x3)
		Genode::error("unaligned kmalloc ", (Genode::addr_t)addr);

	if (flags & __GFP_ZERO)
		Genode::memset(addr, 0, size);

	return addr;
}


void *kzalloc(size_t size, gfp_t flags)
{
	return kmalloc(size, flags | __GFP_ZERO);
}


void *kvzalloc(size_t size, gfp_t flags)
{
	return kmalloc(size, flags | __GFP_ZERO);
}


void *kzalloc_node(size_t size, gfp_t flags, int node)
{
	return kzalloc(size, 0);
}


void *kcalloc(size_t n, size_t size, gfp_t flags)
{
	if (size != 0 && n > (~0UL / size))
		return 0;

	return kzalloc(n * size, flags);
}


void kfree(void const *p)
{
	if (!p) return;

	if (Lx::Malloc::mem().inside((Genode::addr_t)p))
		Lx::Malloc::mem().free(p);
	else if (Lx::Malloc::dma().inside((Genode::addr_t)p))
		Lx::Malloc::dma().free(p);
	else
		Genode::error(__func__, ": unknown block at ", p, ", "
		              "called from ", __builtin_return_address(0));
}


static size_t _ksize(void *p)
{
	size_t size = 0;

	if (Lx::Malloc::mem().inside((Genode::addr_t)p))
		size = Lx::Malloc::mem().size(p);
	else if (Lx::Malloc::dma().inside((Genode::addr_t)p))
		size = Lx::Malloc::dma().size(p);
	else
		Genode::error(__func__, ": unknown block at ", p);

	return size;
}


size_t ksize(void *p)
{
	return _ksize(p);
}


void kzfree(void const *p)
{
	if (!p) return;

	size_t len = ksize(const_cast<void*>(p));

	Genode::memset((void*)p, 0, len);

	kfree(p);
}


void *kmalloc_node_track_caller(size_t size, gfp_t flags, int node)
{
	return kmalloc(size, flags);
}


void *krealloc(void *p, size_t size, gfp_t flags)
{
	/* XXX handle short-cut where size == old_size */
	void *addr = kmalloc(size, flags);

	if (addr && p) {
		size_t old_size = _ksize(p);

		Genode::memcpy(addr, p, old_size);
		kfree(p);
	}

	return addr;
}


void *kmemdup(const void *src, size_t size, gfp_t flags)
{
	void *addr = kmalloc(size, flags);

	if (addr)
		Genode::memcpy(addr, src, size);

	return addr;
}


struct kmem_cache : Lx::Slab_alloc
{
	size_t _object_size;
	void (*ctor)(void *);

	kmem_cache(size_t object_size, bool dma, void (*ctor)(void *))
	:
		Lx::Slab_alloc(object_size, dma ? Lx::Slab_backend_alloc::dma()
		                                : Lx::Slab_backend_alloc::mem()),
		_object_size(object_size),
		ctor(ctor)
	{ }

	size_t size() const { return _object_size; }
};


struct kmem_cache *kmem_cache_create(const char *name, size_t size, size_t align,
                                     unsigned long flags, void (*ctor)(void *))
{
	/*
	 * Copied from wifi_drv.
	 *
	 * XXX SLAB_LX_DMA is never used anywhere else, remove it?
	 */
	enum { SLAB_LX_DMA = 0x80000000ul, };
	return new (Lx::Malloc::mem()) kmem_cache(size, flags & SLAB_LX_DMA, ctor);
}


struct kmem_cache *kmem_cache_create_usercopy(const char *name, size_t size,
                                              size_t align, slab_flags_t flags,
                                              size_t useroffset, size_t usersize,
                                              void (*ctor)(void *))
{
	/* XXX copied from above */
	enum { SLAB_LX_DMA = 0x80000000ul, };
	return new (Lx::Malloc::mem()) kmem_cache(size, flags & SLAB_LX_DMA, ctor);
}


void kmem_cache_destroy(struct kmem_cache *cache)
{
	destroy(Lx::Malloc::mem(), cache);
}


void * kmem_cache_alloc(struct kmem_cache *cache, gfp_t flags)
{
	void * const ptr = cache->alloc_element();

	if (ptr && cache->ctor)
		cache->ctor(ptr);

	return ptr;
}


void kmem_cache_free(struct kmem_cache *cache, void *objp)
{
	cache->free(objp);
}
