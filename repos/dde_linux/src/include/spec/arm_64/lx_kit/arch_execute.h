/**
 * \brief  Platform specific code
 * \author Christian Prochaska
 * \date   2019-07-01
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _ARCH_EXECUTE_H_
#define _ARCH_EXECUTE_H_

#ifdef __cplusplus
extern "C" {
#endif

#define _JBLEN  31
typedef struct _jmp_buf { long _jb[_JBLEN + 1]; } jmp_buf[1];

void _longjmp(jmp_buf, int);
int  _setjmp(jmp_buf);

#ifdef __cplusplus
}
#endif


static inline
void arch_execute(void *sp, void *func, void *arg)
{
	asm volatile ("mov x0, %2;"   /* set arg */
	              "mov sp, %0;"   /* set stack */
	              "mov x29, xzr;" /* clear frame pointer */
	              "br  %1;"       /* call func */
	              ""
	              : : "r"(sp), "r"(func), "r"(arg) : "r0");
}

#endif /* _ARCH_EXECUTE_H_ */
