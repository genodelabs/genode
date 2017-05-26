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

.set CPU_IP,        0
.set CPU_EXCEPTION, 8
.set CPU_X1,        2*8
.set CPU_SP,        3*8
.set CPU_SPTBR,     33*8

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

	# save x30 in master
	sd x30, CPU_X1 + 8 * 28(x31)

	# load kernel page table
	ld x30, CPU_SPTBR(x31)
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
	ld x31, CPU_SPTBR(x30)

	csrw sptbr, x31

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

	sret

# end of the mode transition code
.global _mt_end
_mt_end:
