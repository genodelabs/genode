/*
 * \brief   Mode transition definitions used by all ARM architectures
 * \author  Stefan Kalkowski
 * \date    2014-06-12
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/***************************************************
 ** Constant values that the mode transition uses **
 ***************************************************/

/* kernel names of exceptions that can interrupt a user */
.set RST_TYPE, 1
.set UND_TYPE, 2
.set SVC_TYPE, 3
.set PAB_TYPE, 4
.set DAB_TYPE, 5
.set IRQ_TYPE, 6
.set FIQ_TYPE, 7

.set RST_PC_ADJUST, 0
.set UND_PC_ADJUST, 4
.set SVC_PC_ADJUST, 0
.set PAB_PC_ADJUST, 4
.set DAB_PC_ADJUST, 8
.set IRQ_PC_ADJUST, 4
.set FIQ_PC_ADJUST, 4

/* offsets of the member variables in a processor context */
.set R12_OFFSET,            12 * 4
.set SP_OFFSET,             13 * 4
.set LR_OFFSET,             14 * 4
.set PC_OFFSET,             15 * 4
.set PSR_OFFSET,            16 * 4
.set EXCEPTION_TYPE_OFFSET, 17 * 4
.set TRANSIT_TTBR0_OFFSET,  17 * 4
.set CIDR_OFFSET,           18 * 4
.set TTBR0_OFFSET,          19 * 4

/* size of local variables */
.set CONTEXT_PTR_SIZE, 1 * 4


/*********************************************************
 ** Local data structures that the mode transition uses **
 *********************************************************/

.macro _mt_local_variables

	/* space for a copy of the kernel context */
	.p2align 2
	.global _mt_master_context_begin
	_mt_master_context_begin:
	.space 32 * 4
	.global _mt_master_context_end
	_mt_master_context_end:

	/* space for a client context-pointer per processor */
	.p2align 2
	.global _mt_client_context_ptr
	_mt_client_context_ptr:
	.rept PROCESSORS
		.space CONTEXT_PTR_SIZE
	.endr

	/* a globally mapped buffer per processor */
	.p2align 2
	.global _mt_buffer
	_mt_buffer:
	.rept PROCESSORS
		.space BUFFER_SIZE
	.endr

.endm /* _mt_local_variables */
