/*
 * \brief   Startup code for core
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2011-10-01
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

.include "macros.s"

.section ".text.crt0"

	/* magic multi-boot header to make GRUB happy */
	.long 0x1badb002
	.long 0x0
	.long 0xe4524ffe

	/**********************************
	 ** Startup code for primary CPU **
	 **********************************/

.code32
	.global _start
	_start:

	/* Enable PAE (prerequisite for IA-32e mode) */
	movl %cr4, %eax
	btsl $5, %eax
	movl %eax, %cr4

	/* Load initial pagetables */
	leal _kernel_pml4, %eax
	mov %eax, %cr3

	/* Enable IA-32e mode and execute-disable */
	movl $0xc0000080, %ecx
	rdmsr
	btsl $8, %eax
	btsl $11, %eax
	wrmsr

	/* Enable paging, write protection, caching and FPU error reporting */
	movl %cr0, %eax
	btsl $5, %eax
	btsl $16, %eax
	btrl $29, %eax
	btrl $30, %eax
	btsl $31, %eax
	movl %eax, %cr0

	/* Set up GDT */
	lgdt _gdt_ptr

	/*
	 * Install initial temporary environment that is replaced later by the
	 * environment that init_main_thread creates.
	 */
	leaq _stack_high@GOTPCREL(%rip),%rax
	movq (%rax), %rsp

	/* uniprocessor kernel-initialization which activates multiprocessor */
	call init_kernel_up

	/*********************************************
	 ** Startup code that is common to all CPUs **
	 *********************************************/

	.global _start_secondary_cpus
	_start_secondary_cpus:

	/* do multiprocessor kernel-initialization */
	call init_kernel_mp

	/* call the kernel main-routine */
	call kernel

	/* catch erroneous return of the kernel main-routine */
	1: jmp 1b


/*********************************
 ** .bss (non-initialized data) **
 *********************************/

.bss

	/* stack of the temporary initial environment */
	.p2align 8
	.space 32 * 1024
	_stack_high:

.data

	/****************************************
	 ** Initial pagetables                 **
	 ** Identity mapping from 2MiB to 4MiB **
	 ****************************************/

	/* PML4 */
	.p2align MIN_PAGE_SIZE_LOG2
	_kernel_pml4:
	.quad _kernel_pdp + 0xf
	.fill 511, 8, 0x0

	/* PDP */
	.p2align MIN_PAGE_SIZE_LOG2
	_kernel_pdp:
	.quad _kernel_pd + 0xf
	.fill 511, 8, 0x0

	/* PD */
	.p2align MIN_PAGE_SIZE_LOG2
	_kernel_pd:
	.quad 0
	.quad 0x20018f
	.fill 510, 8, 0x0

	/*******************************************
	 ** Global Descriptor Table               **
	 ** See Intel SDM Vol. 3A, section 3.5.1. **
	 *******************************************/

	.align 4
	.space 2

	_gdt_ptr:
	.word _gdt_end - _gdt_start - 1 /* limit        */
	.long _gdt_start                /* base address */

	.align 8
	_gdt_start:
	/* Null descriptor */
	.quad 0
	/* 64-bit code segment descriptor */
	.long 0
	/* GDTE_LONG | GDTE_PRESENT | GDTE_CODE | GDTE_NON_SYSTEM */
	.long 0x209800
	/* 64-bit data segment descriptor */
	.long 0
	/* GDTE_LONG | GDTE_PRESENT | GDTE_TYPE_DATA_A | GDTE_TYPE_DATA_W | GDTE_NON_SYSTEM */
	.long 0x209300
	/* Task segment descriptor */
	.long 0
	/* GDTE_PRESENT | GDTE_SYS_TSS */
	.long 0x8900
	.long 0
	.long 0
	_gdt_end:
