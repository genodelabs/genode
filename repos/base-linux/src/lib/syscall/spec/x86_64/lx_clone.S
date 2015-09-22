/*
 * \brief  Linux clone() binding
 * \author Christian Prochaska
 * \date   2009-07-14
 *
 * based on glibc-2.9/sysdeps/unix/sysv/linux/x86_64/clone.S
 */

#define L(name) name

#define SYS_clone 56
#define SYS_exit  60

	.text
	.globl lx_clone
	.type lx_clone, @function
lx_clone:
	.cfi_startproc

	/* Align the new stack pointer to 16 bytes. */
	andq    $0xfffffffffffffff0, %rsi

	/* Insert the argument onto the new stack.  */
	subq	$16,%rsi
	movq	%rcx,8(%rsi)

	/* Save the function pointer.  It will be popped off in the 
	   child in the ebx frobbing below.  */
	movq	%rdi,0(%rsi)

	/* Do the system call.  */
	movq	%rdx, %rdi
	movq	%r8, %rdx
	movq	%r9, %r8
	movq	8(%rsp), %r10
	movl	$SYS_clone,%eax

	/* End FDE now, because in the child the unwind info will be 
	   wrong.  */
	.cfi_endproc
	syscall

	testq	%rax,%rax
	jz L(thread_start)

L(pseudo_end):
	/* parent returns */
	ret

L(thread_start):
	.cfi_startproc
	/* Clearing frame pointer is insufficient, use CFI.  */
	.cfi_undefined (%rip);

	/* Clear the frame pointer.  The ABI suggests this be done, to mark 
	   the outermost frame obviously.  */
	xorq	%rbp, %rbp

	/* Set up arguments for the function call.  */
	popq	%rax	/* Function to call.  */
	popq	%rdi	/* Argument.  */
	call	*%rax
	/* Call exit with return value from function call. */
	movq	%rax, %rdi
	movq    $SYS_exit, %rax
	syscall
	.cfi_endproc

/*
 * Allow stacks to be mapped executable (needed because Genode does not
 * offer an API to handle non-executable mappings yet).
 */
.section .note.GNU-stack, "", @progbits

