/*
 * \brief  Linux syscall() binding
 * \author Christian Prochaska
 * \date   2012-05-05
 *
 * based on eglibc-2.11.3/ports/sysdeps/unix/sysv/linux/arm/syscall.S
 *
 * error case:
 *   glibc's syscall() function returns -1 and sets errno
 *   lx_syscall() returns -errno
 */

	.text
	.globl lx_syscall
	.type lx_syscall, #function
lx_syscall:
	mov	ip, sp
	stmfd	sp!, {r4, r5, r6, r7}
	mov	r7, r0
	mov	r0, r1
	mov	r1, r2
	mov	r2, r3
	ldmfd	ip, {r3, r4, r5, r6}
	swi	0x0
	ldmfd	sp!, {r4, r5, r6, r7}
	bx	lr

/* tell the linker that this code does not need an executable stack */
.section .note.GNU-stack, "", %progbits
