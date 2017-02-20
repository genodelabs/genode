/**
 * \brief  Linker script for Linux
 * \author Sebastian Sumpf
 * \date   2015-12-08
 *
 * On Linux 32 bit, we remove the ".text.crt0" section because it contains a
 * text relocation and is not used as startup code for the dynamic linker.
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

SECTIONS
{
  /DISCARD/ : { *(.text.crt0) }
}
