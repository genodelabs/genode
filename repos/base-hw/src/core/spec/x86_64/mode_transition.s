/*
 * \brief  Transition between kernel/userland, and secure/non-secure world
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2011-11-15
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

.include "macros.s"

/* size of pointer to CPU context */
.set CONTEXT_PTR_SIZE, 1 * 8

/* globally mapped buffer storage */
.set BUFFER_SIZE, 6 * 8

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
	.p2align MIN_PAGE_SIZE_LOG2
	.global _mt_begin
	_mt_begin:

	/* space for a copy of the kernel context */
	.p2align 2
	.global _mt_master_context_begin
	_mt_master_context_begin:

	/* space must be at least as large as 'Cpu_state' */
	.space 21*8

	.global _mt_master_context_end
	_mt_master_context_end:

	/* space for a client context-pointer per CPU */
	.p2align 2
	.global _mt_client_context_ptr
	_mt_client_context_ptr:
	.space CONTEXT_PTR_SIZE

	/* a globally mapped buffer per CPU */
	.p2align 2
	.global _mt_buffer
	_mt_buffer:
	.space BUFFER_SIZE

	/*
	 * On user exceptions the CPU has to jump to one of the following
	 * seven entry vectors to switch to a kernel context.
	 */
	.global _mt_kernel_entry_pic
	_mt_kernel_entry_pic:
	1: jmp 1b

	.global _mt_user_entry_pic
	_mt_user_entry_pic:
	1: jmp 1b

	/* end of the mode transition code */
	.global _mt_end
	_mt_end:
	1: jmp 1b
