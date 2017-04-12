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
 * \param transit_ttbr0  transitional TTBR0 value, read/write reg
 * \param new_cidr       new CIDR value, read reg
 * \param new_ttbr0      new TTBR0 value, read/write reg
 */
.macro _switch_protection_domain transit_ttbr0, new_cidr, new_ttbr0

	/*
	 * FIXME: Fixes instability problems that were observed on the
	 *        PandaBoard only. We neither know why invalidating predictions
	 *        at PD switches is a fix nor wether not doing so is the real
	 *        cause of this instability.
	 */
	mcr p15, 0, r0, c7, c5, 6

	mcr p15, 0, \transit_ttbr0, c2, c0, 0
	isb
	mcr p15, 0, \new_cidr, c13, c0, 1
	isb
	mcr p15, 0, \new_ttbr0, c2, c0, 0
	isb
.endm
