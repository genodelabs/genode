/*
 * \brief  GRUB multi-boot information handling
 * \author Christian Helmuth
 * \date   2006-05-10
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <multiboot.h>

using namespace Genode;


unsigned Multiboot_info::num_modules() { return read<Mods_count>(); }


Rom_module Multiboot_info::get_module(unsigned num)
{
	if (num >= num_modules()) return Rom_module();

	Mods mods = _get_mod(num);

	char *cmdline = reinterpret_cast<char*>(mods.read<Mods::Cmdline>());
	/* invalid module -- maybe returned earlier */
	if (!cmdline) return Rom_module();

	/* skip everything in front of the base name of the file */
	for (unsigned i = 0; cmdline[i]; i++) {

		if (cmdline[i] != '/') continue;

		/*
		 * If we detect the end of a directory name, take the
		 * next character as the start of the command line
		 */
		cmdline = cmdline + i + 1;
		i = 0;
	}

	Rom_module ret = Rom_module(mods.read<Mods::Start>(),
	                            mods.read<Mods::End>() - mods.read<Mods::Start>(),
	                            cmdline);

	/* mark module as invalid */
	mods.write<Mods::Cmdline>(0);

	return ret;
}


/**
 * Constructor
 */
Multiboot_info::Multiboot_info(addr_t mbi, bool path_strip)
: Mmio(mbi)
{
	if (!path_strip)
		return;

	/* strip path and arguments from module name */
	for (unsigned i = 0; i < num_modules(); i++) {
		Mods mods = _get_mod(i);
		char *cmdline = reinterpret_cast<char*>(mods.read<Mods::Cmdline>());
		mods.write<Mods::Cmdline>((addr_t)commandline_to_basename(cmdline));
	}
}
