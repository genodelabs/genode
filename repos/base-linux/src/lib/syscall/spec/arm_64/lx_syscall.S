/*
 * \brief  Linux syscall() binding
 * \author Norman Feske
 * \date   2021-04-07
 */

	.text
	.globl lx_syscall
	.type  lx_syscall, #function
lx_syscall:
	stp x29, x30, [sp, #-16]!  /* save sp and link register */
	mov x8, x0                 /* system call number */
	mov x0, x1                 /* arguments ... */
	mov x1, x2
	mov x2, x3
	mov x3, x4
	mov x4, x5
	mov x5, x6
	svc #0                     /* syscall, return value in x0 */
	ldp x29, x30, [sp], #16    /* restore sp and link register */
	ret
