/*
 * \brief   Initial pagetables for x86_64_muen
 * \author  Adrian-Ken Rueegsegger
 * \author  Reto Buerki
 * \date    2015-04-22
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

.include "macros.s"

.data

	/*****************************************
	 ** Identity mapping from 2MiB to 1GiB  **
	 ** plus mappings for Muen pvirt pages  **
	 *****************************************/

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
	.fill 55, 8, 0x0
	.quad _kernel_pd_pvirt + 0xf
	.fill 455, 8, 0x0

	/* PD */
	.p2align MIN_PAGE_SIZE_LOG2
	_kernel_pd:
	.quad 0
	.set entry, 0x20018f
	.rept 511
	.quad entry
	.set entry, entry + 0x200000
	.endr

	.p2align MIN_PAGE_SIZE_LOG2
	_kernel_pd_pvirt:
	.quad 0xe0000018f
	.fill 511, 8, 0x0
