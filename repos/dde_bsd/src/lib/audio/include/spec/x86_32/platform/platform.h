/**
 * \brief  Platform specific code
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2012-06-10
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _X86_32__PLATFORM_H_
#define _X86_32__PLATFORM_H_


#ifdef __cplusplus
extern "C" {
#endif

#define _JBLEN  11
typedef struct _jmp_buf { long _jb[_JBLEN + 1]; } jmp_buf[1];

void _longjmp(jmp_buf, int);
int  _setjmp(jmp_buf);

#ifdef __cplusplus
}
#endif

static inline
void platform_execute(void *sp, void *func, void *arg)
{
	asm volatile ("movl %2, 0(%0);"
	              "movl %1, -0x4(%0);"
	              "movl %0, %%esp;"
	              "call *-4(%%esp);"
	              : : "r" (sp), "r" (func), "r" (arg));
}

#endif /* _X86_32__PLATFORM_H_ */
