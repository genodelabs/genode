/**
 * \brief  Platform specific code
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \author Josef Soentgen
 * \date   2012-06-10
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _X86_64__PLATFORM_H_
#define _X86_64__PLATFORM_H_


#ifdef __cplusplus
extern "C" {
#endif

#define _JBLEN  12
typedef struct _jmp_buf { long _jb[_JBLEN]; } jmp_buf[1];

void    _longjmp(jmp_buf, int);
int     _setjmp(jmp_buf);

#ifdef __cplusplus
}
#endif

static inline
void platform_execute(void *sp, void *func, void *arg)
{
	asm volatile ("movq %2, %%rdi;"
	              "movq %1, 0(%0);"
	              "movq %0, %%rsp;"
	              "call *0(%%rsp);"
	              : "+r" (sp), "+r" (func), "+r" (arg) : : "memory");
}

#endif /* _X86_64__PLATFORM_H_ */
