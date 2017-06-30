/*
 * \brief   GDT macro
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2017-04-17
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/******************************************
 ** Global Descriptor Table (GDT)        **
 ** See Intel SDM Vol. 3A, section 3.5.1 **
 ******************************************/

.macro _define_gdt tss_address
	.align 4
	.space 2
	__gdt_ptr:
	.word 55 /* limit        */
	.long 0  /* base address */

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
	.long (\tss_address & 0xffff) << 16 | TSS_LIMIT
	/* GDTE_PRESENT | GDTE_SYS_TSS */
	.long ((\tss_address >> 24) & 0xff) << 24 | ((\tss_address >> 16) & 0xff) | TSS_TYPE
	.long \tss_address >> 32
	.long 0
	.global __gdt_end
	__gdt_end:
.endm
