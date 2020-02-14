/*
 * \brief  Assembler macros for ARMv7
 * \author Stefan Kalkowski
 * \date   2020-02-16
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

.macro SYSTEM_REGISTER_SYNC_BARRIER
	dsb sy
	isb sy
.endm
