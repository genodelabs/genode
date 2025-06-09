/*
 * \brief   Utilities for creating seL4 kernel objects
 * \author  Alexander Boettcher
 * \date    2025-08-10
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__ARM_V8A_ARCH_KERNEL_OBJECT_H_
#define _CORE__ARM_V8A_ARCH_KERNEL_OBJECT_H_

#include <sel4/objecttype.h>

/* core includes */
#include <types.h>

namespace Core {

	enum {
		PAGE_TABLE_LOG2_SIZE  = 21, /*   2M  region */
		PAGE_DIR_LOG2_SIZE    = 30, /*   1GB region */
	};

	struct Page_table_kobj
	{
		enum { SEL4_TYPE = seL4_ARM_PageTableObject, SIZE_LOG2 = 12 };
		static char const *name() { return "page table"; }
	};


	struct Page_directory_kobj
	{
		enum { SEL4_TYPE = seL4_ARM_PageDirectoryObject, SIZE_LOG2 = 13 };
		static char const *name() { return "page directory"; }
	};

	struct Vspace_kobj
	{
		enum { SEL4_TYPE = seL4_ARM_VSpaceObject, SIZE_LOG2 = 13 };
		static char const *name() { return "ARM vspace table"; }
	};
};

#endif /* _CORE__ARM_V8A_ARCH_KERNEL_OBJECT_H_ */
