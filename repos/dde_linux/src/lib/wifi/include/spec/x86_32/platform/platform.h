/*
 * \brief  Platform specific code
 * \author Sebastian Sumpf
 * \date   2012-06-10
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _X86_32__PLATFORM_H_
#define _X86_32__PLATFORM_H_

static inline
void platform_execute(void *sp, void *func, void *arg)
{
	asm volatile ("movl %2, 0(%0);"
	              "movl %1, -0x4(%0);"
	              "movl %0, %%esp;"
	              "call *-4(%%esp);"
	              : : "r" (sp), "r" (func), "r" (arg));
}

#endif /* _X86_32__PLATFORM_H_ */
