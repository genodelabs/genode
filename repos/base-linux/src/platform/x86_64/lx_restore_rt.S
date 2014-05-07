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

/*
 * Allow stacks to be mapped executable (needed because Genode does not
 * offer an API to handle non-execute mappings yet).
 */
.section .note.GNU-stack, "", @progbits

