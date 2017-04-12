/*
 * \brief   Macros that are used by multiple assembly files
 * \author  Martin Stein
 * \date    2014-01-13
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
.include "spec/arm/macros_support.s"

/**
 * Load kernel name of the executing CPU into register 'r'
 */
.macro _get_cpu_id r

	/* no multiprocessing supported for ARMv6 */
	mov \r, #0
.endm
