/*
 * \brief  Assembler Errors
 * \author  Martin Stein
 * \date  2010-10-06
 *
 * Grouped into one include file for better management and identification
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

.macro _ERROR_NOTHING_LEFT_TO_CALL_BY_CRT0
	brai 0x99000001
.endm


.macro _ERROR_UNKNOWN_USERLAND_BLOCKING_TYPE
	brai 0x99000003
.endm
