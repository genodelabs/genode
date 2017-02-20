/*
 * \brief  Linux-specific linker script additions (STDLIB = no)
 * \author Christian Helmuth
 * \date   2010-09-22
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

PHDRS
{
	stack_area PT_LOAD FLAGS(0);
}

SECTIONS
{
	. = 0x40000000;
	_stack_area_start = .;
	/*
	 * Since Linux loads ldso page aligned, we align the stack area after
	 * loading to a 1 MiB boundary, therefore we reserve one MiB more here.
	 */
	.stack_area : { . += 0x10100000; } : stack_area
}
