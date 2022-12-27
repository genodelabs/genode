/*
 * \brief  Shadow copy of linux/compiler-gcc.h
 * \author Stefan Kalkowski
 * \date   2021-03-17
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__SHADOW__LINUX__COMPILER_GCC_H_
#define _LX_EMUL__SHADOW__LINUX__COMPILER_GCC_H_

#include_next <linux/compiler-gcc.h>

/**
 * We have to re-define `asm_volatile_goto`, because the original function
 * uses `asm goto(...)`, which is a problem when building PIC code.
 */
#ifdef  asm_volatile_goto
#undef  asm_volatile_goto
#define asm_volatile_goto(x...)	asm volatile("invalid use of asm_volatile_goto")
#endif

#endif /* _LX_EMUL__SHADOW__LINUX__COMPILER_GCC_H_ */
