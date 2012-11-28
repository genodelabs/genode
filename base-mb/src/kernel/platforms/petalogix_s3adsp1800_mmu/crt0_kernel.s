/*
 * \brief  Startup code for kernel main routine on microblaze
 * \author Martin Stein
 * \date   2010-06-21
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/*
 * To be compatible to crt0.s for common programs,
 * the following labels are used for roottasks main-thread
 */
.global _main_utcb_addr

/*
 * Here you can denote an instruction pointer to
 * call after kernel execution returns (e.g. userland entry)
 */
.global _call_after_kernel

/* top of kernel stack is used when we have to reset the kernel
 * context, for example when we enter the kernel from userland
 */
.global _kernel_stack_top

/* kernel returns to this label after execution
 */
.global _exit_kernel

/* pointer to kernel main routine */
.extern _kernel



/*******************************
 ** Macros for _start routine **
 *******************************/

.macro _CALL_AFTER_KERNEL__USES_R3_R15
	lwi r3, r0, _call_after_kernel
	beqi r3, _no_call_after_kernel
	addi r15, r0, _exit_call_after_kernel-2*4
	bra r3

	_exit_call_after_kernel:
	_no_call_after_kernel:
.endm


.macro _CALL_KERNEL__USES_R15
	addi r1, r0, _kernel_stack_top
	addi r15, r0, _exit_kernel-2*4
	brai _kernel
	or r0, r0, r0
	_exit_kernel:
.endm



/********************
 ** _start_routine **
 ********************


/* linker links this section to kernelbase + offset 0 */

/* _BEGIN_ELF_ENTRY_CODE */
.global _start
.section ".Elf_entry"
_start:

	_CALL_KERNEL__USES_R15
	_CALL_AFTER_KERNEL__USES_R3_R15

	/* ERROR_NOTHING_LEFT_TO_CALL_BY_CRT0 */
	brai 0x99000001


/* _BEGIN_READABLE_WRITEABLE */
.section ".bss"

	.align 4
	_main_utcb_addr:    .space 4*1
	.align 4
	_call_after_kernel: .space 4*1
	.align 4
	_kernel_stack_base: .space 1024*1024
	_kernel_stack_top:

