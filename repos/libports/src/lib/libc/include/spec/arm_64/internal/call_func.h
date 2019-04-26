/*
 * \brief  User-level task helpers (arm_64)
 * \author Sebastian Sumpf
 * \date   2019-05-06
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__ARM_64__INTERNAL__CALL_FUNC_H_
#define _INCLUDE__SPEC__ARM_64__INTERNAL__CALL_FUNC_H_

/* Libc includes */
#include <setjmp.h>  /* _setjmp() as we don't care about signal state */


/**
 * Call function with a new stack
 */
[[noreturn]] inline void call_func(void *sp, void *func, void *arg)
{
	asm volatile ("mov x0, %2;"   /* set arg */
	              "mov sp, %0;"   /* set stack */
	              "mov x29, xzr;" /* clear frame pointer */
	              "br  %1;"       /* call func */
	              ""
	              : : "r"(sp), "r"(func), "r"(arg) : "x0");
	__builtin_unreachable();
}

#endif /* _INCLUDE__SPEC__ARM_64__INTERNAL__CALL_FUNC_H_ */
