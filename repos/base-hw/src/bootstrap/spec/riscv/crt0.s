/**
 * \brief  Kernel startup code
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \date   2015-06-010
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

.set STACK_SIZE, 64 * 1024

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

la   sp, _stack_area_start
la   a0, STACK_SIZE
ld   a0, (a0)
add  sp, sp, a0

/* save kernel stack pointer in mscratch */
csrw mscratch, sp

jal  setup_riscv_exception_vector
jal  init

1: j 1b

.align 3
_stack_area_start:
.space STACK_SIZE
