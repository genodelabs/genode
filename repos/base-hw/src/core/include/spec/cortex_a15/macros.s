/*
 * \brief   Macros that are used by multiple assembly files
 * \author  Stefan Kalkowski
 * \date    2015-01-30
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
.include "spec/arm_v7/macros_support.s"


/**
 * Switch to a given protection domain
 *
 * There is no atomicity problem when setting the ASID and translation table
 * base address in the one 64-bit TTBR0 register, like in Armv7 cpus without
 * LPAE extensions. Therefore, we don't have to use a transition table.
 *
 * \param transit_ttbr0  ignored parameter
 * \param new_cidr       new CIDR value, read reg
 * \param new_ttbr0      new TTBR0 value, read/write reg
 */
.macro _switch_protection_domain transit_ttbr0, new_cidr, new_ttbr0

	/* write translation-table-base register 0 */
	lsl  \new_cidr, \new_cidr, #16
	mcrr p15, 0, \new_ttbr0, \new_cidr, c2

	/* instruction and data synchronization barrier */
	isb
	dsb
.endm
