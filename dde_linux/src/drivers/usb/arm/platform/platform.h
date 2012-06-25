/**
 * \brief  Platform specific code
 * \author Sebastian Sumpf
 * \date   2012-06-10
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ARM__PLATFORM_H_
#define _ARM__PLATFORM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <platform.h>

static inline
void platform_execute(void *sp, void *func, void *arg)
{
	asm volatile ("mov r0, %2;"  /* set arg */
	              "mov sp, %0;"  /* set stack */
	              "mov pc, %1;"  /* call func */
	              ""
	              : : "r"(sp), "r"(func), "r"(arg) : "r0");
}

#ifdef __cplusplus
}
#endif

#endif /* _ARM__PLATFORM_H_ */
