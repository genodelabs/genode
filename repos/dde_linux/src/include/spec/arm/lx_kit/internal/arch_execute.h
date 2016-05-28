/**
 * \brief  Platform specific code
 * \author Sebastian Sumpf
 * \date   2012-06-10
 */

/*
 * Copyright (C) 2012-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ARCH_EXECUTE_H_
#define _ARCH_EXECUTE_H_

#if defined(USE_INTERNAL_SETJMP)
#ifdef __cplusplus
extern "C" {
#endif

#define _JBLEN  64
typedef struct _jmp_buf { long _jb[_JBLEN + 1]; } jmp_buf[1];

void _longjmp(jmp_buf, int);
int  _setjmp(jmp_buf);

#ifdef __cplusplus
}
#endif
#endif /* USE_INTERNAL_SETJMP */


static inline
void arch_execute(void *sp, void *func, void *arg)
{
	asm volatile ("mov r0, %2;"  /* set arg */
	              "mov sp, %0;"  /* set stack */
	              "mov pc, %1;"  /* call func */
	              ""
	              : : "r"(sp), "r"(func), "r"(arg) : "r0");
}

#endif /* _ARCH_EXECUTE_H_ */
