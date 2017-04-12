/*
 * \brief   Macros that are used by multiple assembly files
 * \author  Stefan Kalkowski
 * \date    2015-01-30
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
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
 * \param ignored        ignored parameter
 * \param ttbr0_low      low word of TTBR0 64-bit register
 * \param ttbr0_high     high word of TTBR0 64-bit register
 */
.macro _switch_protection_domain ignored, ttbr0_low, ttbr0_high

	/* write translation-table-base register 0 */
	mcrr p15, 0, \ttbr0_low, \ttbr0_high, c2

	/* instruction and data synchronization barrier */
	isb
	dsb
.endm
