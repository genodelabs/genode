/*
 * \brief   Utilities for creating seL4 kernel objects
 * \author  Alexander Boettcher
 * \date    2017-07-06
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__X86_ARM_ARCH_KERNEL_OBJECT_H_
#define _CORE__X86_ARM_ARCH_KERNEL_OBJECT_H_

#include <sel4/objecttype.h>

namespace Genode {

	enum {
		PAGE_TABLE_LOG2_SIZE = 20 /* 1M region */
	};

	struct Page_table_kobj
	{
		enum { SEL4_TYPE = seL4_ARM_PageTableObject, SIZE_LOG2 = 12 };
		static char const *name() { return "page table"; }
	};


	struct Page_directory_kobj
	{
		enum { SEL4_TYPE = seL4_ARM_PageDirectoryObject, SIZE_LOG2 = 14 };
		static char const *name() { return "page directory"; }
	};

};

#endif /* _CORE__ARM_ARCH_KERNEL_OBJECT_H_ */
