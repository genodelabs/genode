/*
 * \brief   Startup code for kernel
 * \author  Adrian-Ken Rueegsegger
 * \author  Martin Stein
 * \author  Reto Buerki
 * \author  Stefan Kalkowski
 * \author  Alexander Boettcher
 * \date    2015-02-06
 */

/*
 * Copyright (C) 2011-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

.set STACK_SIZE, 4096

.section ".text.crt0"

	/* magic multi-boot 1 header */
	.long 0x1badb002
	.long 0x0
	.long 0xe4524ffe

	.long 0x0 /* align to 8 byte for mbi2 */
	__mbi2_start:
	/* magic multi-boot 2 header */
	.long   0xe85250d6
	.long   0x0
	.long   (__mbi2_end - __mbi2_start)
	.long  -(0xe85250d6 + (__mbi2_end - __mbi2_start))
	/* end tag - type, flags, size */
	.word   0x0
	.word   0x0
	.long   0x8
	__mbi2_end:


.macro SETUP_PAGING
	/* Enable PAE (prerequisite for IA-32e mode) */
	movl $0x20, %eax
	movl %eax, %cr4

	/* Enable IA-32e mode and execute-disable */
	movl $0xc0000080, %ecx
	rdmsr
	btsl $8, %eax
	btsl $11, %eax
	wrmsr

	/* Enable paging, write protection and caching */
	xorl %eax, %eax
	btsl  $0, %eax /* protected mode */
	btsl $16, %eax /* write protect */
	btsl $31, %eax /* paging */
	movl %eax, %cr0
.endm


	/*******************************************************************
	 ** Startup code for non-primary CPU (AP - application processor) **
	 *******************************************************************/

.code16
	.global _ap
	_ap:

	/* Load initial pagetables */
	mov $_kernel_pml4, %eax
	mov %eax, %cr3

	/* setup paging */
	SETUP_PAGING

	/* setup GDT */
	lgdtl %cs:__gdt - _ap
	ljmpl $8, $_start64

__gdt:
	.word  55
	.long  __gdt_start


	/**************************************************************
	 ** Startup code for primary CPU (bootstrap processor - BSP) **
	 **************************************************************/

.code32
	.global _start
	_start:

	/* preserve multiboot magic value register, used below later */
	movl %eax, %esi

	/**
	 * zero-fill BSS segment
	 */
	leal _bss_start, %edi
	leal _bss_end, %ecx
	sub %edi, %ecx
	shr $2, %ecx
	xor %eax, %eax
	rep stosl

	/* Load initial pagetables */
	leal _kernel_pml4, %eax
	mov %eax, %cr3

	/* setup paging */
	SETUP_PAGING

	/* setup GDT */
	lgdt __gdt_ptr

	/* Indirect long jump to 64-bit code */
	ljmp $8, $_start64_bsp


.code64
	_start64_bsp:

	/* save rax & rbx, used to lookup multiboot structures */
	movq __initial_ax@GOTPCREL(%rip),%rax
	movq %rsi, (%rax)

	movq __initial_bx@GOTPCREL(%rip),%rax
	movq %rbx, (%rax)

	_start64:

	/*
	 * Set up segment selectors fs and gs to zero
	 * ignore ss, cs and ds because they are ignored by hardware in long mode
	 */
	mov $0x0, %eax
	mov %eax, %fs
	mov %eax, %gs

	/* increment CPU counter atomically */
	movq __cpus_booted@GOTPCREL(%rip),%rax
	movq $1, %rcx
	lock xaddq %rcx, (%rax)

	/* if more CPUs started than supported, then stop them */
	cmp $NR_OF_CPUS, %rcx
	jge 1f

	/* calculate stack depending on CPU counter */
	movq $STACK_SIZE, %rax
	inc  %rcx
	mulq %rcx
	movq %rax, %rcx
	leaq __bootstrap_stack@GOTPCREL(%rip),%rax
	movq (%rax), %rsp
	addq %rcx, %rsp

	/*
	 * Enable paging and FPU:
	 * PE, MP, NE, WP, PG
	 */
	mov $0x80010023, %rax
	mov %rax, %cr0

	/*
	 * OSFXSR and OSXMMEXCPT for SSE FPU support
	 */
	mov %cr4, %rax
	bts $9,   %rax
	bts $10,  %rax
	mov %rax, %cr4

	fninit

	/* kernel-initialization */
	call init

	/* catch erroneous return of the kernel initialization */
	1:
	hlt
	jmp 1b


	.global bootstrap_stack_size
	bootstrap_stack_size:
	.quad STACK_SIZE

	/******************************************
	 ** Global Descriptor Table (GDT)        **
	 ** See Intel SDM Vol. 3A, section 3.5.1 **
	 ******************************************/

	.align 4
	.space 2
	__gdt_ptr:
	.word 55 /* limit        */
	.long __gdt_start  /* base address */

	.set TSS_LIMIT, 0x68
	.set TSS_TYPE, 0x8900

	.align 8
	.global __gdt_start
	__gdt_start:
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
	/* 64-bit user code segment descriptor */
	.long 0
	/* GDTE_LONG | GDTE_PRESENT | GDTE_CODE | GDTE_NON_SYSTEM */
	.long 0x20f800
	/* 64-bit user data segment descriptor */
	.long 0
	/* GDTE_LONG | GDTE_PRESENT | GDTE_TYPE_DATA_A | GDTE_TYPE_DATA_W | GDTE_NON_SYSTEM */
	.long 0x20f300
	/* Task segment descriptor */
	.long TSS_LIMIT
	/* GDTE_PRESENT | GDTE_SYS_TSS */
	.long TSS_TYPE
	.long 0
	.long 0
	.global __gdt_end
	__gdt_end:


/*********************************
 ** .bss (non-initialized data) **
 *********************************/

.bss

	/* stack of the temporary initial environment */
	.p2align 12
	.globl __bootstrap_stack
	__bootstrap_stack:
	.rept NR_OF_CPUS
		.space STACK_SIZE
	.endr

	.globl __initial_ax
	__initial_ax:
	.space 8
	.globl __initial_bx
	__initial_bx:
	.space 8
	.globl __cpus_booted
	__cpus_booted:
	.quad 0
