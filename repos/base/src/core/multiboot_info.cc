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

#include <base/printf.h>
#include <multiboot.h>
#include <util/misc_math.h>

using namespace Genode;

namespace Mb_info {
#include "mb_info.h"
}


static const bool verbose = false;


void Multiboot_info::print_debug()
{
	Mb_info::mb_info_t *mbi = (Mb_info::mb_info_t *)_mb_info;

	printf("  flags = %x %s\n", mbi->flags,
	       mbi->flags & MB_CMDLINE ? "CMDLINE" : "");
	printf("  mem_lower = %xu\n",  mbi->mem_lower);
	printf("  mem_upper = %xu\n",  mbi->mem_upper);
	printf("  boot_device = %x\n", mbi->boot_device);
	printf("  mods_count = %d\n",  mbi->mods_count);
	printf("  mods_addr = %xu\n",  mbi->mods_addr);

	unsigned i = 0;
	Mb_info::mb_mod_t *mods = reinterpret_cast<Mb_info::mb_mod_t*>(mbi->mods_addr);
	for (i = 0; i < mbi->mods_count; i++)
		printf("    mod[%02d]  [%xu,%xu) %s\n", i,
		       mods[i].mod_start, (mods[i].mod_end),
		       reinterpret_cast<char*>(mods[i].cmdline));

	printf("  mmap_length = %x\n",       mbi->mmap_length);
	printf("  mmap_addr = %x\n",         mbi->mmap_addr);
	printf("  drives_length = %x\n",     mbi->drives_length);
	printf("  drives_addr = %x\n",       mbi->drives_addr);
	printf("  config_table = %x\n",      mbi->config_table);
	printf("  boot_loader_name = %x\n",  mbi->boot_loader_name);
	printf("  apm_table = %x\n",         mbi->apm_table);
	printf("  vbe_ctrl_info = %xu\n",    mbi->vbe_ctrl_info);
	printf("  vbe_mode_info = %xu\n",    mbi->vbe_mode_info);
	printf("  vbe_mode = %x\n",          mbi->vbe_mode);
	printf("  vbe_interface_seg = %x\n", mbi->vbe_interface_seg);
	printf("  vbe_interface_off = %x\n", mbi->vbe_interface_off);
	printf("  vbe_interface_len = %x\n", mbi->vbe_interface_len);
}


unsigned Multiboot_info::num_modules()
{
	Mb_info::mb_info_t *mbi = (Mb_info::mb_info_t *)_mb_info;

	return mbi->mods_count;
}


Rom_module Multiboot_info::get_module(unsigned num)
{
	Mb_info::mb_info_t *mbi = reinterpret_cast<Mb_info::mb_info_t*>(_mb_info);
	Mb_info::mb_mod_t *mods = reinterpret_cast<Mb_info::mb_mod_t*>(mbi->mods_addr);

	/* num exceeds number of modules */
	if (!(num < mbi->mods_count)) return Rom_module();

	/* invalid module -- maybe returned earlier */
	if (!mods[num].cmdline) return Rom_module();

	char *cmdline = reinterpret_cast<char*>(mods[num].cmdline);

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

	Rom_module ret = Rom_module(mods[num].mod_start,
	                            mods[num].mod_end - mods[num].mod_start,
	                            cmdline);

	/* mark module as invalid */
	mods[num].cmdline = 0;

	return ret;
}


bool Multiboot_info::check_module(unsigned num, addr_t *start, addr_t *end)
{
	Mb_info::mb_info_t *mbi = reinterpret_cast<Mb_info::mb_info_t*>(_mb_info);
	Mb_info::mb_mod_t *mods = reinterpret_cast<Mb_info::mb_mod_t*>(mbi->mods_addr);

	/* num exceeds number of modules */
	if (!(num < mbi->mods_count)) return false;

	*start = mods[num].mod_start;
	*end = mods[num].mod_end;

	return true;
}

/**
 * Constructor
 */
Multiboot_info::Multiboot_info(void *mb_info)
: _mb_info(mb_info)
{
	Mb_info::mb_info_t *mbi = reinterpret_cast<Mb_info::mb_info_t*>(_mb_info);
	Mb_info::mb_mod_t *mods = reinterpret_cast<Mb_info::mb_mod_t*>(mbi->mods_addr);

	/* strip path and arguments from module name */
	for (unsigned i = 0; i < mbi->mods_count; i++) {
		char *cmdline = reinterpret_cast<char*>(mods[i].cmdline);
		mods[i].cmdline = (addr_t)commandline_to_basename(cmdline);
	}

	if (verbose) {
		printf("Multi-boot info with %d modules @ %p.\n",
		       mbi->mods_count, _mb_info);
		print_debug();
	}
}
