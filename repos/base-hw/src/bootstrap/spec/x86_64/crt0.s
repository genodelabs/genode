/*
 * \brief   Startup code for kernel
 * \author  Adrian-Ken Rueegsegger
 * \author  Martin Stein
 * \author  Reto Buerki
 * \author  Stefan Kalkowski
 * \date    2015-02-06
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

.include "hw/spec/x86_64/gdt.s"

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

	/**********************************
	 ** Startup code for primary CPU **
	 **********************************/

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

	/* Enable paging, write protection and caching */
	movl %cr0, %eax
	btsl $16, %eax
	btrl $29, %eax
	btrl $30, %eax
	btsl $31, %eax
	movl %eax, %cr0

	/* Set up GDT */
	movl $__gdt_ptr+2, %eax
	movl $__gdt_start, (%eax)
	lgdt __gdt_ptr

	/* Indirect long jump to 64-bit code */
	ljmp $8, $_start64


.code64
	_start64:

	/*
	 * Set up segment selectors fs and gs to zero
	 * ignore ss, cs and ds because they are ignored by hardware in long mode
	 */
	mov $0x0, %eax
	mov %eax, %fs
	mov %eax, %gs

	/*
	 * Install initial temporary environment that is replaced later by the
	 * environment that init_main_thread creates.
	 */
	leaq _stack_high@GOTPCREL(%rip),%rax
	movq (%rax), %rsp

	movq __initial_ax@GOTPCREL(%rip),%rax
	movq %rsi, (%rax)

	movq __initial_bx@GOTPCREL(%rip),%rax
	movq %rbx, (%rax)

	/* kernel-initialization */
	call init

	/* catch erroneous return of the kernel initialization */
	1: jmp 1b

	_define_gdt 0

/*********************************
 ** .bss (non-initialized data) **
 *********************************/

.bss

	/* stack of the temporary initial environment */
	.p2align 8
	.space 32 * 1024
	_stack_high:
	.globl __initial_ax
	__initial_ax:
	.space 8
	.globl __initial_bx
	__initial_bx:
	.space 8
