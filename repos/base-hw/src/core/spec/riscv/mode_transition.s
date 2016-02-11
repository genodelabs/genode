/*
 * \brief  Transition between kernel/userland
 * \date   2011-11-15
 */

/*
 * Copyright (C) 2011-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */
.set USER_MODE,       0
.set SUPERVISOR_MODE, 1
.set MACHINE_MODE,    3

.set CALL_PUT_CHAR,      0xff

.set CPU_IP,        0
.set CPU_EXCEPTION, 8
.set CPU_X1,        2*8
.set CPU_SP,        3*8
.set CPU_SASID,     33*8
.set CPU_SPTBR,     34*8

.macro _save_scratch_registers mode

	.if \mode == USER_MODE
		csrrw sp, mscratch, sp
	.endif

	addi sp, sp, -24
	sd t0, 0(sp)
	sd t1, 8(sp)
	sd t2, 16(sp)
.endm

.macro  _restore_scratch_registers mode
	ld t0, 0(sp)
	ld t1, 8(sp)
	ld t2, 16(sp)
	addi sp, sp, 24

	.if \mode == USER_MODE
		csrrw sp, mscratch, sp
	.endif
.endm

.macro _put_char mode

	/* check if ecall (8 - 11) */
	csrr t0, mcause
	li   t1, 8
	bltu t0, t1, 9f
	li   t1, 12
	bgtu t0, t1, 9f

	/* check for put char ecall number */
	li t1, CALL_PUT_CHAR
	bne t1, a0, 9f

	/* output character */
	csrw mtohost, a1

1:
	li t0, 0
	csrrw t0, mfromhost, t0
	beqz t0, 1b

	/* advance epc */
	csrr t0, mepc
	addi t0, t0, 4
	csrw mepc, t0

	_restore_scratch_registers \mode
	eret
9:
.endm

.section .text

/*
 * Page aligned base of mode transition code.
 *
 * This position independent code switches between a kernel context and a
 * user context and thereby between their address spaces. Due to the latter
 * it must be mapped executable to the same region in every address space.
 * To enable such switching, the kernel context must be stored within this
 * region, thus one should map it solely accessable for privileged modes.
 */



.p2align 8
.global _machine_begin
_machine_begin:

/* 0x100 user mode */
j user_trap
.space 0x3c
/* 0x140 supervisor */
j supervisor_trap
.space 0x3c
/* 0x180 hypervisor */
1: j 1b
.space 0x3c
/* 0x1c0 machine */
j machine_trap
.space 0x38
/* 0x1fc non-maksable interrupt */
1: j 1b

user_trap:

	_save_scratch_registers USER_MODE
	_put_char USER_MODE
	_restore_scratch_registers USER_MODE
	mrts

supervisor_trap:

	_save_scratch_registers SUPERVISOR_MODE
	_put_char SUPERVISOR_MODE
	j fault

machine_trap:

	_save_scratch_registers MACHINE_MODE
	_put_char MACHINE_MODE
	j fault


fault:j fault /* TODO: handle trap from supervisor or machine mode */

.global _machine_end
_machine_end:

.p2align 12
.global _mt_begin
_mt_begin:

/* 0x100 user mode */
	j  _mt_kernel_entry_pic
.space 0x3c
/* 0x140 supervisor */
1: j 1b
.space 0x3c
/* 0x180 hypervisor */
1: j 1b
.space 0x3c
/* 0x1c0 machine */
1: j 1b
.space 0x38
/* 0x1fc non-maksable interrupt */
1: j 1b

/* space for a client context-pointer per CPU */
.p2align 2
.global _mt_client_context_ptr
_mt_client_context_ptr:
.space 8

/* space for a copy of the kernel context */
.global _mt_master_context_begin
_mt_master_context_begin:

/* space must be at least as large as 'Context' */
.space 35*8

.global _mt_master_context_end
_mt_master_context_end:

.global _mt_kernel_entry_pic
_mt_kernel_entry_pic:

	/* master context */
	csrrw x31, sscratch, x31
	addi  x31, x31, 8

	/* save x29, x30 in master */
	sd x29, CPU_X1 + 8 * 28(x31)
	sd x30, CPU_X1 + 8 * 29(x31)

	/* load kernel page table */
	ld x29, CPU_SASID(x31)
	ld x30, CPU_SPTBR(x31)

	csrw sasid, x29
	csrw sptbr, x30

	/* save x29 - x31 in user context */
	mv   x29, x31
	addi x29, x29, -8
	ld   x29, (x29)

	.irp reg,29,30
		ld x30, CPU_X1 + 8 * (\reg - 1)(x31)
		sd x30, CPU_X1 + 8 * (\reg - 1)(x29)
	.endr

	csrr x30, sscratch /* x31 */
	sd x30, CPU_X1 + 8 * 30(x29)

	/* save x1 - x28 */
	.irp reg,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28
		sd x\reg, CPU_X1 + 8 * (\reg - 1)(x29)
	.endr

	/* trap reason */
	csrr x30, scause
	sd   x30, CPU_EXCEPTION(x29)

	/* ip */
	csrr x30, sepc
	sd   x30, CPU_IP(x29)

	/* load kernel stack and ip */
	ld sp,  CPU_SP(x31)
	ld x30, CPU_IP(x31)

	/* restore scratch */
	addi x31, x31, -8
	csrw sscratch, x31

	jalr x30


.global _mt_user_entry_pic
_mt_user_entry_pic:

	/* client context pointer */
	csrr x30, sscratch
	ld   x30, (x30)

	/* set return IP */
	ld x31, CPU_IP(x30)
	csrw sepc, x31

	/* restore x1-x28 */
	.irp reg,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28
		ld x\reg, CPU_X1 + 8 * (\reg - 1)(x30)
	.endr

	/* save x29, x30, x31 to master context */
	csrr x29, sscratch
	addi x29, x29, 8 /* master context */

	.irp reg,29,30,31
		ld x31, CPU_X1 + 8 * (\reg - 1)(x30)
		sd x31, CPU_X1 + 8 * (\reg - 1)(x29)
	.endr

	/* switch page table */
	ld x31, CPU_SASID(x30)
	ld x30, CPU_SPTBR(x30)

	csrw sasid, x31
	csrw sptbr, x30

	/* restore x29 - x31 from master context */
	.irp reg,31,30,29
		ld x\reg, CPU_X1 + 8 * (\reg - 1)(x29)
	.endr

	eret

/* end of the mode transition code */
.global _mt_end
_mt_end:
