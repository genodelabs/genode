/*
 * \brief   Utilities for creating seL4 kernel objects
 * \author  Norman Feske
 * \date    2015-05-08
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__X86_32_ARCH_KERNEL_OBJECT_H_
#define _CORE__X86_32_ARCH_KERNEL_OBJECT_H_

#include <sel4/objecttype.h>
#include <platform.h>

namespace Genode {

	Phys_allocator &phys_alloc_16k(Allocator * core_mem_alloc = nullptr);

	enum {
		PAGE_TABLE_LOG2_SIZE = 22 /* 4M region */
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

	struct Vcpu_kobj
	{
		enum { SEL4_TYPE = seL4_X86_VCPUObject, SIZE_LOG2 = 14 };
		static char const *name() { return "vcpu"; }
	};

	struct Ept_kobj
	{
		enum { SEL4_TYPE = seL4_X86_EPTPML4Object, SIZE_LOG = 12 };
		static char const *name() { return "ept pml4"; }
	};
};

#endif /* _CORE__X86_32_ARCH_KERNEL_OBJECT_H_ */
