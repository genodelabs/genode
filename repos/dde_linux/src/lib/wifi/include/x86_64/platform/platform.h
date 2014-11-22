/**
 * \brief  Platform specific code
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

#ifndef _X86_64__PLATFORM_H_
#define _X86_64__PLATFORM_H_


static inline
void platform_execute(void *sp, void *func, void *arg)
{
	asm volatile ("movq %2, %%rdi;"
	              "movq %1, 0(%0);"
	              "movq %0, %%rsp;"
	              "call *0(%%rsp);"
	              : "+r" (sp), "+r" (func), "+r" (arg) : : "memory");
}

#endif /* _X86_64__PLATFORM_H_ */
