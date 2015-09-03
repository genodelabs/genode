/*
 * \brief  Linux clone() binding
 * \author Christian Prochaska
 * \date   2012-05-05
 *
 * based on eglibc-2.11.3/ports/sysdeps/unix/sysv/linux/arm/clone.S
 */

#define SYS_clone 120
#define SYS_exit 1
#define SYS_getpid 20

#define __ARM_EABI__  1

#define CLONE_VM      0x00000100
#define CLONE_THREAD  0x00010000

	.text
	.globl lx_clone
	.type lx_clone, #function
lx_clone:

	@ insert the args onto the new stack
	str	r3, [r1, #-4]!
	str	r0, [r1, #-4]!

	@ do the system call
	@ get flags
	mov	r0, r2
#ifdef RESET_PID
	mov	ip, r2
#endif
	@ new sp is already in r1
#ifdef __ARM_EABI__
	stmfd	sp!, {r4, r7}
#else
	str	r4, [sp, #-8]!
#endif
	ldr	r2, [sp, #8]
	ldr	r3, [sp, #12]
	ldr	r4, [sp, #16]
#ifdef __ARM_EABI__
	ldr	r7, =SYS_clone
	swi	0x0
#else
	swi	SYS_clone
#endif
	cmp	r0, #0
	beq	1f
#ifdef __ARM_EABI__
	ldmfd	sp!, {r4, r7}
#else
	ldr	r4, [sp], #8
#endif
	#blt	PLTJMP(C_SYMBOL_NAME(__syscall_error))
	bx lr

1:
#ifdef RESET_PID
	tst	ip, #CLONE_THREAD
	bne	3f
	mov	r0, #0xffff0fff
	mov	lr, pc
	sub	pc, r0, #31
	mov	r1, r0
	tst	ip, #CLONE_VM
	movne	r0, #-1
#ifdef __ARM_EABI__
	ldr	r7, =SYS_getpid
	swieq	0x0
#else
	swieq	SYS_getpid
#endif
	str	r0, [r1, #PID_OFFSET]
	str	r0, [r1, #TID_OFFSET]
3:
#endif
	@ pick the function arg and call address off the stack and execute
	ldr	r0, [sp, #4]
#if defined(__ARM_ARCH_4T__) && defined(__THUMB_INTERWORK__)
	ldr 	ip, [sp], #8
	mov	lr, pc
	bx      ip
#else
	mov	lr, pc
	ldr 	pc, [sp], #8
#endif

	@ and we are done, passing the return value through r0
	ldr	r7, =SYS_exit
	swi	0x0

/* tell the linker that this code does not need an executable stack */
.section .note.GNU-stack, "", %progbits
