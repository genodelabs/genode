/*
 * \brief  User-level task helpers (arm)
 * \author Christian Helmuth
 * \date   2016-01-22
 */

/*
 * Copyright (C) 2016-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* libc-internal includes */
#include <internal/call_func.h>


/**
 * Call function with a new stack
 */
[[noreturn]] void call_func(void *sp, void *func, void *arg)
{
	asm volatile ("mov r0, %2;"  /* set arg */
	              "mov sp, %0;"  /* set stack */
	              "mov fp, #0;"  /* clear frame pointer */
	              "mov pc, %1;"  /* call func */
	              ""
	              : : "r"(sp), "r"(func), "r"(arg) : "r0");
	__builtin_unreachable();
}
