/**
 * \brief  Kernel startup code
 * \author Sebastian Sumpf
 * \date   2015-06-010
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

.section ".text.crt0"

.global _start
_start:

la   sp, kernel_stack
la   a0, kernel_stack_size
ld   a0, (a0)
add  sp, sp, a0

jal  init_kernel
