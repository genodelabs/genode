/*
 * \brief  Transition between kernel/userland
 * \author Sebastian Sumpf
 * \author Mark Vels
 * \date   2015-06-22
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

.set USER_MODE,       0
.set SUPERVISOR_MODE, 1
.set MACHINE_MODE,    3

.set CALL_PUT_CHAR,      0x100
.set CALL_SET_SYS_TIMER, 0x101
.set CALL_IS_USER_MODE,  0x102

.set CPU_IP,        0
.set CPU_EXCEPTION, 8
.set CPU_X1,        2*8
.set CPU_SP,        3*8
.set CPU_SASID,     33*8
.set CPU_SPTBR,     34*8


# From encoding.h (riscv-opcode)
.set MIP_MTIP,      0x00000020
.set MIP_SSIP,      0x00000002
.set MIP_HSIP,      0x00000004
.set MIP_MSIP,      0x00000008
.set MIP_STIP,      0x00000020
.set MIP_HTIP,      0x00000040
.set MIP_MTIP,      0x00000080
.set MSTATUS_IE,    0x00000001
.set MSTATUS_PRV,   0x00000006
.set MSTATUS_IE1,   0x00000008
.set MSTATUS_PRV1,  0x00000030
.set MSTATUS_IE2,   0x00000040
.set MSTATUS_PRV2,  0x00000180
.set MSTATUS_IE3,   0x00000200
.set MSTATUS_PRV3,  0x00000C00
.set MSTATUS_FS,    0x00003000
.set MSTATUS_XS,    0x0000C000
.set MSTATUS_MPRV,  0x00010000
.set MSTATUS_VM,    0x003E0000
.set MSTATUS64_SD,  0x8000000000000000

.set TRAP_ECALL_FROM_USER,        8
.set TRAP_ECALL_FROM_SUPERVISOR,  9
.set TRAP_ECALL_FROM_HYPERVISOR, 10
.set TRAP_ECALL_FROM_MACHINE,    11

.set TRAP_INTERRUPT_BITNR,    63
.set IRQ_SOFT,                0x0
.set IRQ_TIMER,               0x1
.set IRQ_HOST,                0x2
.set IRQ_COP,                 0x3



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

.macro _handle_trap mode

	csrr t0, mcause

	# If IRQ bit not setup, goto trap handler.
	# If an interrupt has occurred, the MSB will be set and
	# hence mcause will be negative
	#
	bgez t0, 11f

	# The bit was not set so we're handling an interrupt
	# Valid interrupts are :
	# - Software IRQ    - 0
	# - Timer IRQ       - 1
	# - HOST  HTIF      - 2
	# - COP             - 3
	#

	sll t0, t0, 1    # discard MSB

	# If interrupt source is IRQ TIMER .... 
	li t1, IRQ_TIMER * 2
	bne t0, t1, 2f

	# Forward handling of timer IRQ to SUPERVISOR
	li t0, MIP_MTIP
	csrc mip, t0
	csrc mie, t0
	li t1, MIP_STIP
	csrs mip, t1

	# If irq from supervisor and MSTATUS.IE1 is not set,
	# then bail out using 'eret'
	#
        .if \mode == SUPERVISOR_MODE
		csrr t1, mstatus
		and t0, t1, MSTATUS_IE1
		bne zero, t0, 1f

		# So, IE1 is not set.
		_restore_scratch_registers \mode
		eret

        .endif

1:
	# should cause a interrupt trap in supervisor mode
	_restore_scratch_registers \mode
	mrts
2:
	# If interrupt source is IRQ HOST .... 
	li t1, IRQ_HOST * 2
	bne t0, t1, 9f

3:
	# Empty mfromhost
	li t0, 0
	csrrw t0, mfromhost, t0
	bne zero,t0, 3b
	j 9f

	# Future implementation check for more interrupt sources
	# to handle here.....

9:
	#********  IRQ OUT *********
	_restore_scratch_registers \mode
	eret

11:
	# Handle trap

	# check if ecall (8..11):
	# 8  : Environment call from U-mode
	# 9  : Environment call from S-mode
	# 10 : Environment call from H-mode
	# 11 : Environment call from M-mode
	#
	# If not, jump to end of macro.
	#

	li   t1, TRAP_ECALL_FROM_USER
	bltu t0, t1, 19f
	li   t1, TRAP_ECALL_FROM_MACHINE
	bgt  t0, t1, 19f

	# Switch on ecall number
	li t1, CALL_PUT_CHAR
	beq t1, a0, 12f

	li t1, CALL_SET_SYS_TIMER
	beq t1, a0, 13f

	li t1, CALL_IS_USER_MODE
	beq t1, a0, 14f

	# else, unknown ecall number
	.if \mode == USER_MODE
		# Assume that Genode (supervisor trap handler)
		# knows what to do then.
		_restore_scratch_registers \mode
		mrts
	.endif
	j 15f

12:
	# output character but first wait until mtohost reads 0 atomically
	# to make sure any previous character is gone..
	csrr t1, mtohost
	bne zero, t1, 12b

	csrw mtohost, a1
	j 15f

13:
	# Only allow timer fiddling from supervisor mode
	.if \mode == SUPERVISOR_MODE
		# Clear any pending STIP
		li t0, MIP_STIP
		csrc mip, t0

		# Set system timer
		csrw mtimecmp, a1

		# enable timer interrupt in M-mode
		li t0, MIP_MTIP
		csrrs t0, mie, t0
	.endif
	j 15f

14:
	mv a0, x0
	.if \mode == USER_MODE
		li a0, 1
	.endif
	j 15f

15:
	#*******  ECALL OUT *********
	# Empty mfromhost 
	li t0, 0
	csrrw t0, mfromhost, t0
	bne zero,t0, 14b

	# advance epc
	csrr t0, mepc
	addi t0, t0, 4
	csrw mepc, t0

	_restore_scratch_registers \mode
	eret
19:
.endm

.section .text

##
 # Page aligned base of mode transition code.
 #
 # This position independent code switches between a kernel context and a
 # user context and thereby between their address spaces. Due to the latter
 # it must be mapped executable to the same region in every address space.
 # To enable such switching, the kernel context must be stored within this
 # region, thus one should map it solely accessable for privileged modes.
 #



.p2align 8
.global _machine_begin
_machine_begin:

# 0x100 user mode
j user_trap
.space 0x3c
# 0x140 supervisor
j supervisor_trap
.space 0x3c
# 0x180 hypervisor
1: j 1b
.space 0x3c
# 0x1c0 machine
j machine_trap
.space 0x38
# 0x1fc non-maksable interrupt
1: j 1b

user_trap:

	_save_scratch_registers USER_MODE
	_handle_trap USER_MODE
	_restore_scratch_registers USER_MODE
	mrts

supervisor_trap:

	_save_scratch_registers SUPERVISOR_MODE
	_handle_trap SUPERVISOR_MODE
	j fault

machine_trap:

	_save_scratch_registers MACHINE_MODE
	_handle_trap MACHINE_MODE
	j fault


fault:j fault # TODO: handle trap from supervisor or machine mode

.global _machine_end
_machine_end:

.p2align 12
.global _mt_begin
_mt_begin:

# 0x100 user mode
j  _mt_kernel_entry_pic
.space 0x3c
# 0x140 supervisor
1: j 1b
.space 0x3c
# 0x180 hypervisor
1: j 1b
.space 0x3c
# 0x1c0 machine
1: j 1b
.space 0x38
# 0x1fc non-maksable interrupt
1: j 1b

/* space for a client context-pointer per CPU */
.p2align 3
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

	# master context
	csrrw x31, sscratch, x31
	addi  x31, x31, 8

	# save x29, x30 in master
	sd x29, CPU_X1 + 8 * 28(x31)
	sd x30, CPU_X1 + 8 * 29(x31)

	# load kernel page table
	ld x29, CPU_SASID(x31)
	ld x30, CPU_SPTBR(x31)

	csrw sasid, x29
	csrw sptbr, x30

	#
	# FIXME
	# A TLB flush. Might be necessary to remove this in the near future again
	# because on real hardware we currently get problems without.
	#
	sfence.vm x0

	# save x29 - x31 in user context 
	mv   x29, x31
	addi x29, x29, -8
	ld   x29, (x29)

	.irp reg,29,30
		ld x30, CPU_X1 + 8 * (\reg - 1)(x31)
		sd x30, CPU_X1 + 8 * (\reg - 1)(x29)
	.endr

	csrr x30, sscratch /* x31 */
	sd x30, CPU_X1 + 8 * 30(x29)

	# save x1 - x28
	.irp reg,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28
		sd x\reg, CPU_X1 + 8 * (\reg - 1)(x29)
	.endr

	# trap reason
	csrr x30, scause
	sd   x30, CPU_EXCEPTION(x29)

	# ip
	csrr x30, sepc
	sd   x30, CPU_IP(x29)

	# load kernel stack and ip
	ld sp,  CPU_SP(x31)
	ld x30, CPU_IP(x31)

	# restore scratch
	addi x31, x31, -8
	csrw sscratch, x31

	jalr x30


.global _mt_user_entry_pic
_mt_user_entry_pic:

	# client context pointer
	csrr x30, sscratch
	ld   x30, (x30)

	# set return IP
	ld x31, CPU_IP(x30)
	csrw sepc, x31

	# restore x1-x28
	.irp reg,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28
		ld x\reg, CPU_X1 + 8 * (\reg - 1)(x30)
	.endr

	# save x29, x30, x31 to master context
	csrr x29, sscratch
	addi x29, x29, 8 # master context

	.irp reg,29,30,31
		ld x31, CPU_X1 + 8 * (\reg - 1)(x30)
		sd x31, CPU_X1 + 8 * (\reg - 1)(x29)
	.endr

	# switch page table
	ld x31, CPU_SASID(x30)
	ld x30, CPU_SPTBR(x30)

	csrw sasid, x31
	csrw sptbr, x30

	#
	# FIXME
	# A TLB flush. Might be necessary to remove this in the near future again
	# because on real hardware we currently get problems without.
	#

	sfence.vm x0

	# restore x29 - x31 from master context 
	.irp reg,31,30,29
		ld x\reg, CPU_X1 + 8 * (\reg - 1)(x29)
	.endr

	eret

# end of the mode transition code
.global _mt_end
_mt_end:
