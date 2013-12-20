/*
 * \brief  Allocator for core-local memory
 * \author Norman Feske
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

/* local includes */
#include <core_mem_alloc.h>
#include <util.h>

using namespace Genode;

static const bool verbose_core_mem_alloc = false;


bool Core_mem_allocator::Mapped_mem_allocator::alloc(size_t size, void **out_addr)
{
	/* try to allocate block in cores already mapped virtual address ranges */
	if (_alloc.alloc(size, out_addr))
		return true;

	/* there is no sufficient space in core's mapped virtual memory, expansion needed */
	size_t page_rounded_size = (size + get_page_size() - 1) & get_page_mask();
	void *phys_addr = 0, *virt_addr = 0;

	/* allocate physical pages */
	if (!_phys_alloc->alloc(page_rounded_size, &phys_addr)) {
		PERR("Could not allocate physical memory region of size %zu\n", page_rounded_size);
		return false;
	}

	/* allocate range in core's virtual address space */
	if (!_virt_alloc->alloc(page_rounded_size, &virt_addr)) {
		PERR("Could not allocate virtual address range in core of size %zu\n", page_rounded_size);

		/* revert physical allocation */
		_phys_alloc->free(phys_addr);
		return false;
	}

	if (verbose_core_mem_alloc)
		printf("added core memory block of %zu bytes at virt=%p phys=%p\n",
		       page_rounded_size, virt_addr, phys_addr);

	/* make physical page accessible at the designated virtual address */
	_map_local((addr_t)virt_addr, (addr_t)phys_addr, get_page_size_log2());

	/* add new range to core's allocator for mapped virtual memory */
	_alloc.add_range((addr_t)virt_addr, page_rounded_size);

	/* now that we have added enough memory, try again... */
	return _alloc.alloc(size, out_addr);
}
