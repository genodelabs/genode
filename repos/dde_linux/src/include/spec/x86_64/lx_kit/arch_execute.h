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
void arch_execute(void *sp, void *func, void *arg)
{
	asm volatile ("movq %0,  %%rsp;"     /* load stack pointer */
	              "movq %%rsp, %%rbp;"   /* caller stack frame (for GDB debugging) */
	              "movq %0, -8(%%rbp);"
	              "movq %1, -16(%%rbp);"
	              "movq %2, -24(%%rbp);"
	              "sub  $24, %%rsp;"     /* adjust to next stack frame */
	              "movq %2, %%rdi;"      /* 1st argument */
	              "call *-16(%%rbp);"    /* call func */
	              : : "r" (sp), "r" (func), "r" (arg));
}

#endif /* _ARCH_EXECUTE_H_ */
