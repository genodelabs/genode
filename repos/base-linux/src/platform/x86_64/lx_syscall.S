/*
 * \brief  Linux syscall() binding
 * \author Christian Prochaska
 * \date   2009-07-14
 *
 * based on glibc-2.9/sysdeps/unix/sysv/linux/x86_64/syscall.S
 *
 * error case:
 *   glibc's syscall() function returns -1 and sets errno
 *   lx_syscall() returns -errno
 */


#define L(name) name

	.text
	.globl lx_syscall
	.type lx_syscall, @function
lx_syscall:
	movq %rdi, %rax		/* Syscall number -> rax.  */
	movq %rsi, %rdi		/* shift arg1 - arg5.  */
	movq %rdx, %rsi
	movq %rcx, %rdx
	movq %r8, %r10
	movq %r9, %r8
	movq 8(%rsp),%r9	/* arg6 is on the stack.  */
	syscall			    /* Do the system call.  */
L(pseudo_end):
	ret            /* Return to caller.  */

/*
 * Allow stacks to be mapped executable (needed because Genode does not
 * offer an API to handle non-execute mappings yet).
 */
.section .note.GNU-stack, "", @progbits

