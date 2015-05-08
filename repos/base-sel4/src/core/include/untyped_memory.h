/*
 * \brief   Utilities for dealing with untyped memory
 * \author  Norman Feske
 * \date    2015-05-06
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__UNTYPED_MEMORY_H_
#define _CORE__INCLUDE__UNTYPED_MEMORY_H_

/* Genode includes */
#include <base/allocator.h>

/* core includes */
#include <util.h>
#include <untyped_address.h>

/* seL4 includes */
#include <sel4/interfaces/sel4_client.h>

namespace Genode { struct Untyped_memory; }

struct Genode::Untyped_memory
{
	class Phys_alloc_failed : Exception { };


	/**
	 * Allocate natually-aligned physical memory for seL4 kernel object
	 *
	 * \throw Phys_alloc_failed
	 * \throw Untyped_address::Lookup_failed
	 */
	static inline Untyped_address alloc_log2(Range_allocator &phys_alloc,
	                                         size_t const size_log2)
	{
		/*
		 * The natual alignment is needed to ensure that the backing store is
		 * contained in a single untyped memory region.
		 */
		void *out_ptr = nullptr;
		size_t const size = 1UL << size_log2;
		Range_allocator::Alloc_return alloc_ret =
			phys_alloc.alloc_aligned(size, &out_ptr, size_log2);
		addr_t const phys_addr = (addr_t)out_ptr;

		if (alloc_ret.is_error()) {
			PERR("%s: allocation of untyped memory failed", __FUNCTION__);
			throw Phys_alloc_failed();
		}

		return Untyped_address(phys_addr, size);
	}


	/**
	 * Allocate natually aligned physical memory
	 *
	 * \param size  size in bytes
	 */
	static inline Untyped_address alloc(Range_allocator &phys_alloc,
	                                    size_t const size)
	{
		if (size == 0) {
			PERR("%s: invalid size of 0x%zd", __FUNCTION__, size);
			throw Phys_alloc_failed();
		}

		/* calculate next power-of-two size that fits the allocation size */
		size_t const size_log2 = log2(size - 1) + 1;

		return alloc_log2(phys_alloc, size_log2);
	}


	/**
	 * Create page frames from untyped memory
	 */
	static inline void convert_to_page_frames(addr_t phys_addr,
	                                          size_t num_pages)
	{
		size_t const size = num_pages << get_page_size_log2();

		/* align allocation offset to page boundary */
		Untyped_address const untyped_address(align_addr(phys_addr, 12), size);

		seL4_Untyped const service     = untyped_address.sel();
		int          const type        = seL4_IA32_4K;
		int          const offset      = untyped_address.offset();
		int          const size_bits   = 0;
		seL4_CNode   const root        = Core_cspace::TOP_CNODE_SEL;
		int          const node_index  = Core_cspace::TOP_CNODE_PHYS_IDX;
		int          const node_depth  = Core_cspace::NUM_TOP_SEL_LOG2;
		int          const node_offset = phys_addr >> get_page_size_log2();
		int          const num_objects = num_pages;

		int const ret = seL4_Untyped_RetypeAtOffset(service,
		                                            type,
		                                            offset,
		                                            size_bits,
		                                            root,
		                                            node_index,
		                                            node_depth,
		                                            node_offset,
		                                            num_objects);

		if (ret != 0) {
			PERR("%s: seL4_Untyped_RetypeAtOffset (IA32_4K) returned %d",
			     __FUNCTION__, ret);
		}
	}

	static inline unsigned frame_sel(addr_t phys_addr)
	{
		return (Core_cspace::TOP_CNODE_PHYS_IDX << Core_cspace::NUM_PHYS_SEL_LOG2)
		     | (phys_addr >> get_page_size_log2());
	}
};

#endif /* _CORE__INCLUDE__UNTYPED_MEMORY_H_ */
