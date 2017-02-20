/*
 * \brief   Startup code for core
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2011-10-01
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/************
 ** Macros **
 ************/

/* core includes */
.include "macros.s"

.section ".text.crt0"

	/**********************************
	 ** Startup code for primary CPU **
	 **********************************/

	.global _start
	_start:

	/* setup temporary stack pointer */
	_get_constraints_of_kernel_stacks r2, r3
	_init_kernel_sp r2, r3

	/* kernel-initialization */
	bl init_kernel

	/* catch erroneous return of the kernel initialization routine */
	1: b 1b
