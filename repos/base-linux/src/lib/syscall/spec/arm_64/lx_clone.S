/*
 * \brief  Linux clone() binding
 * \author Norman Feske
 * \date   2021-04-12
 */

#define SYS_clone 220

	.text
	.globl lx_clone
	.type lx_clone, #function
lx_clone:
	stp x29, x30, [sp, #-16]!  /* save sp and link register */
	stp x3, x0, [x1, #-16]!    /* supply fn and argp at new thread's stack */
	mov x0, x2                 /* flags */
	mov w8, #SYS_clone
	svc #0                     /* syscall, return value in x0 */
	ldp x29, x30, [sp], #16    /* restore sp and link register */
	ret
