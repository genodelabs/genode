/*
 * \brief  User-level task helpers (arm)
 * \author Christian Helmuth
 * \date   2016-01-22
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__ARM__INTERNAL__CALL_FUNC_H_
#define _INCLUDE__SPEC__ARM__INTERNAL__CALL_FUNC_H_

/* Libc includes */
#include <setjmp.h>  /* _setjmp() as we don't care about signal state */


/**
 * Call function with a new stack
 */
[[noreturn]] inline void call_func(void *sp, void *func, void *arg)
{
	asm volatile ("mov r0, %2;"  /* set arg */
	              "mov sp, %0;"  /* set stack */
	              "mov fp, #0;"  /* clear frame pointer */
	              "mov pc, %1;"  /* call func */
	              ""
	              : : "r"(sp), "r"(func), "r"(arg) : "r0");
	__builtin_unreachable();
}

#endif /* _INCLUDE__SPEC__ARM__INTERNAL__CALL_FUNC_H_ */
