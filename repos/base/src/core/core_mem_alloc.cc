/*
 * \brief  Allocator for core-local memory
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2009-10-12
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread.h>

/* local includes */
#include <core_mem_alloc.h>

using namespace Core;


void * Mapped_avl_allocator::map_addr(void * addr)
{
	Block *b = static_cast<Block *>(_find_by_address((addr_t)addr));

	if(!b || !b->used()) return 0;

	size_t off = (addr_t)addr - b->addr();
	return (void*) (((addr_t)b->map_addr) + off);
}


Range_allocator::Alloc_result
Mapped_mem_allocator::alloc_aligned(size_t size, unsigned align, Range range)
{
	size_t page_rounded_size = align_addr(size, get_page_size_log2());
	align = max((int)align, (int)get_page_size_log2());

	/* allocate physical pages */
	return _phys_alloc->alloc_aligned(page_rounded_size, align, range)
	                   .convert<Alloc_result>(

		[&] (void *phys_addr) -> Alloc_result {

			/* allocate range in core's virtual address space */
			return _virt_alloc->alloc_aligned(page_rounded_size, align)
			                   .convert<Alloc_result>(

				[&] (void *virt_addr) {

					_phys_alloc->metadata(phys_addr, { virt_addr });
					_virt_alloc->metadata(virt_addr, { phys_addr });

					/* make physical page accessible at the designated virtual address */
					_map_local((addr_t)virt_addr, (addr_t)phys_addr, page_rounded_size);

					return virt_addr;
				},
				[&] (Alloc_error e) {
					error("Could not allocate virtual address range in core of size ",
					      page_rounded_size, " (error ", (int)e, ")");

					/* revert physical allocation */
					_phys_alloc->free(phys_addr);
					return e;
				});
		},
		[&] (Alloc_error e) {
			error("Could not allocate physical memory region of size ",
			      page_rounded_size, " (error ", (int)e, ")");
			return e;
		});
}


void Mapped_mem_allocator::free(void *addr, size_t)
{
	using Block = Mapped_avl_allocator::Block;
	Block *b = static_cast<Block *>(_virt_alloc->_find_by_address((addr_t)addr));
	if (!b) return;

	if (!_unmap_local((addr_t)addr, (addr_t)b->map_addr, b->size())) {
		error("error on unmap virt=", addr, " phys=",
		      Hex_range<addr_t>((addr_t)b->map_addr, b->size()));

		/* leak physical and virtual region because of unknown usage state */
		return;
	}

	_phys_alloc->free(b->map_addr, b->size());
	_virt_alloc->free(addr, b->size());
}


void Mapped_mem_allocator::free(void *)
{
	warning(__func__, "not implemented!");
}
