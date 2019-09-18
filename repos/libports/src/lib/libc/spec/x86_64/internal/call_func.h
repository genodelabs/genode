/*
 * \brief  User-level task helpers (x86_64)
 * \author Christian Helmuth
 * \date   2016-01-22
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__X86_64__INTERNAL__CALL_FUNC_H_
#define _INCLUDE__SPEC__X86_64__INTERNAL__CALL_FUNC_H_

/* Libc includes */
#include <setjmp.h>  /* _setjmp() as we don't care about signal state */


/**
 * Call function with a new stack
 */
[[noreturn]] inline void call_func(void *sp, void *func, void *arg)
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
	__builtin_unreachable();
}

#endif /* _INCLUDE__SPEC__X86_64__INTERNAL__CALL_FUNC_H_ */
