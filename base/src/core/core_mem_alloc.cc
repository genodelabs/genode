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

/* local includes */
#include <core_mem_alloc.h>

using namespace Genode;

static const bool verbose_core_mem_alloc = false;


Range_allocator::Alloc_return
Core_mem_allocator::alloc_aligned(size_t size, void **out_addr, int align)
{
	size_t page_rounded_size = (size + get_page_size() - 1) & get_page_mask();
	void  *phys_addr = 0;
	align = max((size_t)align, get_page_size_log2());

	/* allocate physical pages */
	Alloc_return ret1 = _phys_alloc.raw()->alloc_aligned(page_rounded_size,
	                                                     &phys_addr, align);
	if (!ret1.is_ok()) {
		PERR("Could not allocate physical memory region of size %zu\n",
		     page_rounded_size);
		return ret1;
	}

	/* allocate range in core's virtual address space */
	Alloc_return ret2 = _virt_alloc.raw()->alloc_aligned(page_rounded_size,
	                                                     out_addr, align);
	if (!ret2.is_ok()) {
		PERR("Could not allocate virtual address range in core of size %zu\n",
		     page_rounded_size);

		/* revert physical allocation */
		_phys_alloc.raw()->free(phys_addr);
		return ret2;
	}

	if (verbose_core_mem_alloc)
		printf("added core memory block of %zu bytes at virt=%p phys=%p\n",
		       page_rounded_size, *out_addr, phys_addr);

	/* make physical page accessible at the designated virtual address */
	_map_local((addr_t)*out_addr, (addr_t)phys_addr, page_rounded_size);

	return Alloc_return::OK;
}
