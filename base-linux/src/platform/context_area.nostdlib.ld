/*
 * \brief  Linux-specific linker script additions (STDLIB = no)
 * \author Christian Helmuth
 * \date   2010-09-22
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

PHDRS
{
	context_area PT_LOAD FLAGS(0);
}

SECTIONS
{
	. = 0x40000000;
	_context_area_start = .;
	.context_area : { . += 0x10000000; } : context_area
	_context_area_end = .;
}
