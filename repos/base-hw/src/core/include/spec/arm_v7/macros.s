/*
 * \brief   Macros that are used by multiple assembly files
 * \author  Martin Stein
 * \date    2014-01-13
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
.include "spec/arm/macros_support.s"

/**
 * Determine the kernel name of the executing processor
 *
 * \param target_reg  register that shall receive the processor name
 */
.macro _get_processor_id target_reg

	/* read the multiprocessor affinity register */
	mrc p15, 0, \target_reg, c0, c0, 5

	/* get the affinity-0 bitfield from the read register value */
	and \target_reg, \target_reg, #0xff
.endm
