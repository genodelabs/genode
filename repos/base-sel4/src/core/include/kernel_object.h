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

/* Genode includes */
#include <base/exception.h>

/* core includes */
#include <untyped_memory.h>
#include <initial_untyped_pool.h>

namespace Genode {

	/**
	 * Index referring to a slot in a CNode
	 */
	struct Cnode_index : Cap_sel
	{
		explicit Cnode_index(addr_t value) : Cap_sel(value) { }

		Cnode_index(Cap_sel sel) : Cap_sel(sel.value()) { }
	};


	struct Tcb_kobj
	{
		enum { SEL4_TYPE = seL4_TCBObject, SIZE_LOG2 = 12 };
		static char const *name() { return "TCB"; }
	};


	struct Endpoint_kobj
	{
		enum { SEL4_TYPE = seL4_EndpointObject, SIZE_LOG2 = 4 };
		static char const *name() { return "endpoint"; }
	};


	struct Notification_kobj
	{
		enum { SEL4_TYPE = seL4_NotificationObject, SIZE_LOG2 = 4 };
		static char const *name() { return "notification"; }
	};


	struct Cnode_kobj
	{
		enum { SEL4_TYPE = seL4_CapTableObject, SIZE_LOG2 = 4 };
		static char const *name() { return "cnode"; }
	};


	struct Page_table_kobj
	{
		enum { SEL4_TYPE = seL4_X86_PageTableObject, SIZE_LOG2 = 12 };
		static char const *name() { return "page table"; }
	};


	struct Page_directory_kobj
	{
		enum { SEL4_TYPE = seL4_X86_PageDirectoryObject, SIZE_LOG2 = 12 };
		static char const *name() { return "page directory"; }
	};


	struct Retype_untyped_failed : Genode::Exception { };


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
	 * \return physical address of created kernel object
	 *
	 * \throw Phys_alloc_failed
	 * \throw Retype_untyped_failed
	 *
	 * The kernel-object description is a policy type that contains enum
	 * definitions for 'SEL4_TYPE' and 'SIZE_LOG2', and a static function
	 * 'name' that returns the type name as a char const pointer.
	 *
	 * XXX to be removed
	 */
	template <typename KOBJ>
	static addr_t create(Range_allocator &phys_alloc,
	                     Cap_sel          dst_cnode_sel,
	                     Cnode_index      dst_idx,
	                     size_t           size_log2 = 0)
	{
		/* allocate physical page */
		addr_t phys_addr = Untyped_memory::alloc_page(phys_alloc);

		seL4_Untyped const service     = Untyped_memory::untyped_sel(phys_addr).value();
		int          const type        = KOBJ::SEL4_TYPE;
		int          const size_bits   = size_log2;
		seL4_CNode   const root        = dst_cnode_sel.value();
		int          const node_index  = 0;
		int          const node_depth  = 0;
		int          const node_offset = dst_idx.value();
		int          const num_objects = 1;

		int const ret = seL4_Untyped_Retype(service,
		                                    type,
		                                    size_bits,
		                                    root,
		                                    node_index,
		                                    node_depth,
		                                    node_offset,
		                                    num_objects);

		if (ret != 0) {
			error("seL4_Untyped_RetypeAtOffset (", KOBJ::name(), ") "
			      "returned ", ret);
			throw Retype_untyped_failed();
		}

		return phys_addr;
	}


	/**
	 * Create kernel object from initial untyped memory pool
	 *
	 * \param KOBJ           kernel-object description
	 * \param untyped_pool   initial untyped memory pool
	 * \param dst_cnode_sel  CNode selector where to store the cap pointing to
	 *                       the new kernel object
	 * \param dst_idx        designated index of cap selector within 'dst_cnode'
	 * \param size_log2      size of kernel object in bits, only needed for
	 *                       variable-sized objects like CNodes
	 *
	 * \throw Initial_untyped_pool::Initial_untyped_pool_exhausted
	 * \throw Retype_untyped_failed
	 *
	 * The kernel-object description is a policy type that contains enum
	 * definitions for 'SEL4_TYPE' and 'SIZE_LOG2', and a static function
	 * 'name' that returns the type name as a char const pointer.
	 */
	template <typename KOBJ>
	static void create(Initial_untyped_pool &untyped_pool,
	                   Cap_sel               dst_cnode_sel,
	                   Cnode_index           dst_idx,
	                   size_t                size_log2 = 0)
	{
		unsigned const sel = untyped_pool.alloc(KOBJ::SIZE_LOG2 + size_log2);

		seL4_Untyped const service     = sel;
		int          const type        = KOBJ::SEL4_TYPE;
		int          const size_bits   = size_log2;
		seL4_CNode   const root        = dst_cnode_sel.value();
		int          const node_index  = 0;
		int          const node_depth  = 0;
		int          const node_offset = dst_idx.value();
		int          const num_objects = 1;

		int const ret = seL4_Untyped_Retype(service,
		                                    type,
		                                    size_bits,
		                                    root,
		                                    node_index,
		                                    node_depth,
		                                    node_offset,
		                                    num_objects);

		if (ret != 0) {
			error("seL4_Untyped_Retype (", KOBJ::name(), ") returned ", ret);
			throw Retype_untyped_failed();
		}
	}
};

#endif /* _CORE__INCLUDE__KERNEL_OBJECT_H_ */
