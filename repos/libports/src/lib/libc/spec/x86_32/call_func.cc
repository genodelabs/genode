/*
 * \brief  User-level task helpers (x86_32)
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
	asm volatile ("movl %2, 0(%0);"
	              "movl %1, -0x4(%0);"
	              "movl %0, %%esp;"
	              "xorl %%ebp, %%ebp;"  /* clear frame pointer */
	              "call *-4(%%esp);"
	              : : "r" (sp), "r" (func), "r" (arg));
	__builtin_unreachable();
}
