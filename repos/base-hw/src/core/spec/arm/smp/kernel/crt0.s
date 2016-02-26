/*
 * \brief   Startup code for SMP kernel
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2015-12-08
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/************
 ** Macros **
 ************/

/* core includes */
.include "macros.s"

.section ".text.crt0"

/*********************************************
 ** Startup code that is common to all CPUs **
 *********************************************/

.global _start_secondary_cpus
_start_secondary_cpus:

/* setup multiprocessor-aware kernel stack-pointer */
_get_constraints_of_kernel_stacks r0, r1
_init_kernel_sp r0, r1

/* do multiprocessor kernel-initialization */
bl init_kernel_mp

/* catch erroneous return of the kernel main-routine */
1: b 1b
