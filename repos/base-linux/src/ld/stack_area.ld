/*
 * \brief  Linux-specific linker script additions
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
	/*
	 * Start of Linux binary and stack area
	 */
	. = 0x40000000; /* 1GB */
	_stack_area_start = .;

	/*
	 * 256 MB
	 */
	.stack_area : { . += 0x10000000; } : stack_area
}
