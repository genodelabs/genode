/**
 * \brief  Platform specific code
 * \author Sebastian Sumpf
 * \date   2012-06-10
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _X86_32__PLATFORM_H_
#define _X86_32__PLATFORM_H_

#include <platform.h>

static inline
void platform_execute(void *sp, void *func, void *arg)
{
	asm volatile ("movl %2, 0(%0);"
	              "movl %1, -0x4(%0);"
	              "movl %0, %%esp;"
	              "call *-4(%%esp);"
	              : : "r" (sp), "r" (func), "r" (arg));
}

extern "C" void module_ehci_hcd_init();
extern "C" void module_uhci_hcd_init();

inline void platform_hcd_init(Services *s)
{

	/* ehci_hcd should always be loaded before uhci_hcd and ohci_hcd, not after */
	module_ehci_hcd_init();
	module_uhci_hcd_init();
}

#endif /* _X86_32__PLATFORM_H_ */
