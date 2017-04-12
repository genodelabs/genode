/*
 * \brief   Macros that are used by multiple assembly files
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2014-01-13
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

.include "hw/spec/x86_64/gdt.s"

/***************************************************
 ** Constant values that are pretty commonly used **
 ***************************************************/

/* alignment constraints */
.set MIN_PAGE_SIZE_LOG2, 12
.set DATA_ACCESS_ALIGNM_LOG2, 2
