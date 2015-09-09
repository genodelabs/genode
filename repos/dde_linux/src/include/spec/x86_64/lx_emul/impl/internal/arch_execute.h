/*
 * \brief  Architecture-specific code
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \date   2012-06-10
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ARCH_EXECUTE_H_
#define _ARCH_EXECUTE_H_

static inline
void arch_execute(void *sp, void *func, void *arg)
{
	asm volatile ("movq %2, %%rdi;"
	              "movq %1, 0(%0);"
	              "movq %0, %%rsp;"
	              "call *0(%%rsp);"
	              : "+r" (sp), "+r" (func), "+r" (arg) : : "memory");
}

#endif /* _ARCH_EXECUTE_H_ */
