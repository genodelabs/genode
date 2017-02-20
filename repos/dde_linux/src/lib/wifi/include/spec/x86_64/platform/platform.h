/*
 * \brief  Platform specific code
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \date   2012-06-10
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _X86_64__PLATFORM_H_
#define _X86_64__PLATFORM_H_


static inline
void platform_execute(void *sp, void *func, void *arg)
{
	asm volatile ("movq %0,  %%rsp;"     /* load stack pointer */
	              "movq %%rsp, %%rbp;"   /* caller stack frame (for GDB debugging) */
	              "movq %0, -8(%%rbp);"
	              "movq %1, -16(%%rbp);"
	              "movq %2, -24(%%rbp);"
	              "sub  $24, %%rsp;"     /* adjust to next stack frame */
	              "movq %2, %%rdi;"      /* 1st argument */
	              "call *-16(%%rbp);"    /* call func */
	              : : "r" (sp), "r" (func), "r" (arg));
}

#endif /* _X86_64__PLATFORM_H_ */
