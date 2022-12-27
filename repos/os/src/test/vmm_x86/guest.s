/*
 * \brief  VM session interface test for x86: guest code
 * \author Alexander Boettcher
 * \author Christian Helmuth
 * \date   2018-09-26
 *
 * The guest code is placed on the page hosting the reset vector 0xffff:fff0.
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

.code16

/* offset code start to keep the jmp distance in imm8 */
.org 0xf7e, 0x90
1:
	rdtscp
	hlt
	hlt
	jmp .
	hlt

	jmp .

/* reset vector entry - just jmp to code start above */
.org 0xff0, 0x90
	jmp 1b

/* fill page with hlt */
.org 0x1000, 0xf4
