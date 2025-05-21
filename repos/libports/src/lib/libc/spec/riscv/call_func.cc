/*
 * \brief  User-level task helpers (riscv)
 * \author Sebastian Sumpf
 * \date   2021-06-29
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
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
	asm volatile ("mv a0, %2\n" /* set arg */
	              "mv sp, %0\n" /* set stack */
	              "mv fp, x0\n" /* clear frame pointer */
	              "jalr %1\n"   /* call func */
	              ""
	              : : "r"(sp), "r"(func), "r"(arg) : "a0");
	__builtin_unreachable();
}
