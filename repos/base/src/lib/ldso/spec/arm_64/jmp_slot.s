/**
 * \brief  Jump slot entry code for ARM 64-bit platforms
 * \author Stefan Kalkowski
 * \date   2019-03-27
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

.text
.globl _jmp_slot
.type  _jmp_slot,%function

_jmp_slot:
	1: b 1b
