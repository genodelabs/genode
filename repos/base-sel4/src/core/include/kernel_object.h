/*
 * \brief   Utilities for creating seL4 kernel objects
 * \author  Norman Feske
 * \date    2015-05-08
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__KERNEL_OBJECT_H_
#define _CORE__INCLUDE__KERNEL_OBJECT_H_

/* core includes */
#include <untyped_memory.h>

namespace Kernel_object {

	using Genode::Untyped_address;
	using Genode::Untyped_memory;
	using Genode::Range_allocator;

	struct Tcb
	{
		enum { SEL4_TYPE = seL4_TCBObject, SIZE_LOG2 = 12 };
		static char const *name() { return "TCB"; }
	};


	struct Endpoint
	{
		enum { SEL4_TYPE = seL4_EndpointObject, SIZE_LOG2 = 4 };
		static char const *name() { return "endpoint"; }
	};


	struct Cnode
	{
		enum { SEL4_TYPE = seL4_CapTableObject, SIZE_LOG2 = 4 };
		static char const *name() { return "cnode"; }
	};


	struct Page_table
	{
		enum { SEL4_TYPE = seL4_IA32_PageTableObject, SIZE_LOG2 = 12 };
		static char const *name() { return "page table"; }
	};


	struct Page_directory
	{
		enum { SEL4_TYPE = seL4_IA32_PageDirectoryObject, SIZE_LOG2 = 12 };
		static char const *name() { return "page directory"; }
	};


	/**
	 * Create kernel object from untyped memory
	 *
	 * \param KOBJ           kernel-object description
	 * \param phys_alloc     allocator for untyped memory
	 * \param dst_cnode_sel  CNode selector where to store the cap pointing to
	 *                       the new kernel object
	 * \param dst_idx        designated index of cap selector within 'dst_cnode'
	 * \param size_log2      size of kernel object in bits, only needed for
	 *                       variable-sized objects like CNodes
	 *
	 * \throw Phys_alloc_failed
	 * \throw Untyped_address::Lookup_failed
	 *
	 * The kernel-object description is a policy type that contains enum
	 * definitions for 'SEL4_TYPE' and 'SIZE_LOG2', and a static function
	 * 'name' that returns the type name as a char const pointer.
	 */
	template <typename KOBJ>
	static Untyped_address create(Range_allocator &phys_alloc,
	                              unsigned         dst_cnode_sel,
	                              unsigned         dst_idx,
	                              size_t           size_log2 = 0)
	{
		Untyped_address const untyped_addr =
			Untyped_memory::alloc_log2(phys_alloc, KOBJ::SIZE_LOG2 + size_log2);

		seL4_Untyped const service     = untyped_addr.sel();
		int          const type        = KOBJ::SEL4_TYPE;
		int          const offset      = untyped_addr.offset();
		int          const size_bits   = size_log2;
		seL4_CNode   const root        = dst_cnode_sel;
		int          const node_index  = 0;
		int          const node_depth  = 0;
		int          const node_offset = dst_idx;
		int          const num_objects = 1;

		int const ret = seL4_Untyped_RetypeAtOffset(service,
		                                            type,
		                                            offset,
		                                            size_bits,
		                                            root,
		                                            node_index,
		                                            node_depth,
		                                            node_offset,
		                                            num_objects);

		if (ret != 0)
			PERR("seL4_Untyped_RetypeAtOffset (%s) returned %d",
			     KOBJ::name(), ret);

		return untyped_addr;
	}
};

#endif /* _CORE__INCLUDE__KERNEL_OBJECT_H_ */
