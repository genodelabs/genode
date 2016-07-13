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
#include <cap_sel_alloc.h>

/* seL4 includes */
#include <sel4/sel4.h>

namespace Genode { struct Untyped_memory; }

struct Genode::Untyped_memory
{
	class Phys_alloc_failed : Exception { };


	static inline addr_t alloc_pages(Range_allocator &phys_alloc, size_t num_pages)
	{
		void *out_ptr = nullptr;
		Range_allocator::Alloc_return alloc_ret =
			phys_alloc.alloc_aligned(num_pages*get_page_size(), &out_ptr,
			                         get_page_size_log2());

		if (alloc_ret.error()) {
			error(__PRETTY_FUNCTION__, ": allocation of untyped memory failed");
			throw Phys_alloc_failed();
		}

		return (addr_t)out_ptr;
	}


	static inline addr_t alloc_page(Range_allocator &phys_alloc)
	{
		return alloc_pages(phys_alloc, 1);
	}


	static inline void free_page(Range_allocator &phys_alloc, Genode::addr_t addr)
	{
		phys_alloc.free(reinterpret_cast<void *>(addr));
	}


	/**
	 * Local utility solely used by 'untyped_sel' and 'frame_sel'
	 */
	static inline Cap_sel _core_local_sel(Core_cspace::Top_cnode_idx top_idx,
	                                      addr_t phys_addr)
	{
		unsigned const upper_bits = top_idx << Core_cspace::NUM_PHYS_SEL_LOG2;
		unsigned const lower_bits = phys_addr >> get_page_size_log2();

		return Cap_sel(upper_bits | lower_bits);
	}


	/**
	 * Return core-local selector for untyped page at given physical address
	 */
	static inline Cap_sel untyped_sel(addr_t phys_addr)
	{
		return _core_local_sel(Core_cspace::TOP_CNODE_UNTYPED_IDX, phys_addr);
	}


	/**
	 * Return core-local selector for 4K page frame at given physical address
	 */
	static inline Cap_sel frame_sel(addr_t phys_addr)
	{
		return _core_local_sel(Core_cspace::TOP_CNODE_PHYS_IDX, phys_addr);
	}


	/**
	 * Create page frames from untyped memory
	 */
	static inline void convert_to_page_frames(addr_t phys_addr,
	                                          size_t num_pages)
	{
		for (size_t i = 0; i < num_pages; i++, phys_addr += get_page_size()) {

			seL4_Untyped const service     = untyped_sel(phys_addr).value();
			int          const type        = seL4_X86_4K;
			int          const size_bits   = 0;
			seL4_CNode   const root        = Core_cspace::top_cnode_sel();
			int          const node_index  = Core_cspace::TOP_CNODE_PHYS_IDX;
			int          const node_depth  = Core_cspace::NUM_TOP_SEL_LOG2;
			int          const node_offset = phys_addr >> get_page_size_log2();
			int          const num_objects = 1;

			int const ret = seL4_Untyped_Retype(service,
			                                    type,
			                                    size_bits,
			                                    root,
			                                    node_index,
			                                    node_depth,
			                                    node_offset,
			                                    num_objects);

			if (ret != seL4_NoError) {
				error(__FUNCTION__, ": seL4_Untyped_RetypeAtOffset (IA32_4K) "
				      "returned ", ret);
				return;
			}
		}
	}


	/**
	 * Free up page frames and turn it so into untyped memory
	 */
	static inline void convert_to_untyped_frames(addr_t const phys_addr,
	                                             addr_t const phys_size)
	{
		seL4_Untyped const service = Core_cspace::phys_cnode_sel();
		int const space_size = Core_cspace::NUM_PHYS_SEL_LOG2;

		for (addr_t phys = phys_addr; phys < phys_addr + phys_size;
		     phys += get_page_size()) {

			int const index = phys >> get_page_size_log2();
			/**
			 * Without the revoke, one gets sporadically
			 *  Untyped Retype: Insufficient memory ( xx bytes needed, x bytes
			 *                                        available)
			 * for the phys_addr when it gets reused.
			 */
			int ret = seL4_CNode_Revoke(service, index, space_size);
			if (ret != seL4_NoError)
				error(__FUNCTION__, ": seL4_CNode_Revoke returned ", ret);

			/**
			 * Without the delete, one gets:
			 *  Untyped Retype: Slot #xxxx in destination window non-empty
			 */
			ret = seL4_CNode_Delete(service, index, space_size);
			if (ret != seL4_NoError)
				error(__FUNCTION__, ": seL4_CNode_Delete returned ", ret);
		}
	}
};

#endif /* _CORE__INCLUDE__UNTYPED_MEMORY_H_ */
