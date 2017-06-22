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

#ifndef _CORE__X86_64_ARCH_KERNEL_OBJECT_H_
#define _CORE__X86_64_ARCH_KERNEL_OBJECT_H_

#include <sel4/objecttype.h>

namespace Genode {

	enum {
		PAGE_TABLE_LOG2_SIZE  = 21, /*   2M  region */
		PAGE_DIR_LOG2_SIZE    = 30, /*   1GB region */
		PAGE_PDPT_LOG2_SIZE   = 39  /* 512GB region */
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

	struct Page_pointer_table_kobj
	{
		enum { SEL4_TYPE = seL4_X86_PDPTObject, SIZE_LOG2 = 12 };
		static char const *name() { return "page directory pointer table"; }
	};

	struct Page_map_kobj
	{
		enum { SEL4_TYPE = seL4_X64_PML4Object, SIZE_LOG2 = 12 };
		static char const *name() { return "page-map level-4 table"; }
	};
};

#endif /* _CORE__X86_64_ARCH_KERNEL_OBJECT_H_ */
