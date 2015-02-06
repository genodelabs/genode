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
 * Load kernel name of the executing CPU into register 'r'
 */
.macro _get_cpu_id r

	/* read the multiprocessor affinity register */
	mrc p15, 0, \r, c0, c0, 5

	/* get the affinity-0 bitfield from the read register value */
	and \r, \r, #0xff
.endm
