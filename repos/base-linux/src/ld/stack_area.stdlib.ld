/*
 * \brief  Linux-specific linker script additions (STDLIB = yes)
 * \author Christian Helmuth
 * \date   2010-09-22
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

SECTIONS
{
	. = 0x40000000;
	_stack_area_start = .;
	.stack_area : { . += 0x10000000; }
}
