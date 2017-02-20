/*
 * \brief  Architecture-specific code
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \date   2012-06-10
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _ARCH_EXECUTE_H_
#define _ARCH_EXECUTE_H_

#if defined(USE_INTERNAL_SETJMP)
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
#endif /* USE_INTERNAL_SETJMP */


static inline
void arch_execute(void *sp, void *func, void *arg)
{
	asm volatile ("movq %2, %%rdi;"
	              "movq %1, 0(%0);"
	              "movq %0, %%rsp;"
	              "call *0(%%rsp);"
	              : "+r" (sp), "+r" (func), "+r" (arg) : : "memory");
}

#endif /* _ARCH_EXECUTE_H_ */
