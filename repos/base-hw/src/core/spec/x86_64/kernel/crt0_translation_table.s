/*
 * \brief   Initial pagetables for x86_64
 * \author  Adrian-Ken Rueegsegger
 * \author  Reto Buerki
 * \date    2015-04-22
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

.include "macros.s"

.data

	/********************************************
	 ** Identity mapping from 2MiB to 16MiB    **
	 ** plus mappings for LAPIC, I/O APIC MMIO **
	 ********************************************/

	/* PML4 */
	.p2align MIN_PAGE_SIZE_LOG2
	.global _kernel_pml4
	_kernel_pml4:
	.quad _kernel_pdp + 0xf
	.fill 511, 8, 0x0

	/* PDP */
	.p2align MIN_PAGE_SIZE_LOG2
	_kernel_pdp:
	.quad _kernel_pd + 0xf
	.fill 2, 8, 0x0
	.quad _kernel_pd_503 + 0xf
	.fill 508, 8, 0x0

	/* PD */
	.p2align MIN_PAGE_SIZE_LOG2
	_kernel_pd:
	.quad 0
	.quad 0x20018f
	.quad 0x40018f
	.quad 0x60018f
	.quad 0x80018f
	.quad 0xa0018f
	.quad 0xc0018f
	.quad 0xe0018f
	.fill 504, 8, 0x0

	.p2align MIN_PAGE_SIZE_LOG2
	_kernel_pd_503:
	.fill 502, 8, 0x0
	.quad 0xfec0019f
	.quad 0xfee0019f
	.fill 8, 8, 0x0
