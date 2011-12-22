/*
 * \brief  Linux signal handler restorer function
 * \author Christian Prochaska
 * \date   2009-07-14
 *
 */


#define SYS_rt_sigreturn 15

	.text
	.globl lx_restore_rt
	.type lx_restore_rt, @function
lx_restore_rt:
	movq $SYS_rt_sigreturn, %rax
	syscall
