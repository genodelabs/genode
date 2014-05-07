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


/**
 * Determine the kernel name of the executing processor
 *
 * \param target_reg  register that shall receive the processor name
 */
.macro _get_processor_id target_reg

	/* no multiprocessing supported for ARMv6 */
	mov \target_reg, #0
.endm
