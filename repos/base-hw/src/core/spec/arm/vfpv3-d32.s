/*
 * \brief  VFPv3-D32 context load/store
 * \author Stefan Kalkowski
 * \date   2018-05-06
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

.section .text

.global vfp_save_fpu_context
vfp_save_fpu_context:
	push   { r1, lr }
	vmrs   r1, fpscr
	stmia  r0!, {r1}
	vstm   r0!, {d0-d15}
	vstm   r0!, {d16-d31}
	pop    { r1, pc }


.global vfp_load_fpu_context
vfp_load_fpu_context:
	push { r1, lr }
	ldr  r1, [r0]
	vmsr fpscr, r1
	add  r1, r0, #4
	vldm r1!, {d0-d15}
	vldm r1!, {d16-d31}
	pop  { r1, pc }
