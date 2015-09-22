/*
 * \brief  Allocator for core-local memory
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2009-10-12
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/thread.h>

/* local includes */
#include <core_mem_alloc.h>

using namespace Genode;

static const bool verbose_core_mem_alloc = false;


void * Core_mem_allocator::Mapped_avl_allocator::map_addr(void * addr)
{
	Block *b = static_cast<Block *>(_find_by_address((addr_t)addr));

	if(!b || !b->used()) return 0;

	size_t off = (addr_t)addr - b->addr();
	return (void*) (((addr_t)b->map_addr) + off);
}


Range_allocator::Alloc_return
Core_mem_allocator::Mapped_mem_allocator::alloc_aligned(size_t size, void **out_addr, int align, addr_t from, addr_t to)
{
	size_t page_rounded_size = (size + get_page_size() - 1) & get_page_mask();
	void  *phys_addr = 0;
	align = max((size_t)align, get_page_size_log2());

	/* allocate physical pages */
	Alloc_return ret1 = _phys_alloc->alloc_aligned(page_rounded_size,
	                                               &phys_addr, align, from, to);
	if (!ret1.is_ok()) {
		PERR("Could not allocate physical memory region of size %zu\n",
		     page_rounded_size);
		return ret1;
	}

	/* allocate range in core's virtual address space */
	Alloc_return ret2 = _virt_alloc->alloc_aligned(page_rounded_size,
	                                               out_addr, align);
	if (!ret2.is_ok()) {
		PERR("Could not allocate virtual address range in core of size %zu\n",
		     page_rounded_size);

		/* revert physical allocation */
		_phys_alloc->free(phys_addr);
		return ret2;
	}

	if (verbose_core_mem_alloc)
		printf("added core memory block of %zu bytes at virt=%p phys=%p\n",
		       page_rounded_size, *out_addr, phys_addr);

	_phys_alloc->metadata(phys_addr, { *out_addr });
	_virt_alloc->metadata(*out_addr, { phys_addr });

	/* make physical page accessible at the designated virtual address */
	_map_local((addr_t)*out_addr, (addr_t)phys_addr, page_rounded_size);

	return Alloc_return::OK;
}


void Core_mem_allocator::Mapped_mem_allocator::free(void *addr, size_t size)
{
	using Block = Mapped_avl_allocator::Block;
	Block *b = static_cast<Block *>(_virt_alloc->_find_by_address((addr_t)addr));
	if (!b) return;

	_unmap_local((addr_t)addr, b->size());
	_phys_alloc->free(b->map_addr, b->size());
	_virt_alloc->free(addr, b->size());
}


void Core_mem_allocator::Mapped_mem_allocator::free(void *addr)
{
	PWRN("Not implemented!");
}
