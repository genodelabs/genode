/**
 * \brief  Kernel startup code
 * \author Sebastian Sumpf
 * \date   2015-06-010
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

.section ".text.crt0"

.global _start
_start:
j _start_next_page

/* leave first page empty for mode-transition page located at 0x100 */
.space 4096

_start_next_page:

 /* clear the bss segment */
la   a0, _bss_start
la   a1, _bss_end
1:
sd   x0, (a0)
addi a0, a0, 8
bne  a0, a1, 1b

la   sp, kernel_stack
la   a0, kernel_stack_size
ld   a0, (a0)
add  sp, sp, a0

/* save kernel stack pointer in mscratch */
csrw mscratch, sp

jal  setup_riscv_exception_vector
jal  init_kernel

1: j 1b
