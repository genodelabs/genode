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

.section ".text.crt0"

# 0x100 user mode
j  _kernel_entry

.p2align 8

_kernel_entry:

	# client context
	csrrw x31, sscratch, x31

	# save x1 - x30
	.irp reg,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30
		sd x\reg, CPU_X1 + 8 * (\reg - 1)(x31)
	.endr

	# trap reason
	csrr x30, scause
	sd   x30, CPU_EXCEPTION(x31)

	# ip
	csrr x30, sepc
	sd   x30, CPU_IP(x31)

	# x31
	csrr x30, sscratch
	sd   x30, CPU_X1 + 8 * 30(x31)

	la x29, kernel_stack
	la x30, kernel_stack_size
	ld x30, (x30)
	add sp, x29, x30
	la x30, kernel

	jalr x30


.section .text

	/*******************************
	 ** idle loop for idle thread **
	 *******************************/

	.global idle_thread_main
	idle_thread_main:
	wfi
	j idle_thread_main
