/*
 * \brief  Default implementation of the ROM modules initialization
 * \author Martin Stein
 * \date   2017-08-17
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <boot_modules.h>
#include <platform.h>

using namespace Genode;


void Platform::_init_rom_modules()
{
	/* add boot modules to ROM FS */
	Boot_modules_header *header = &_boot_modules_headers_begin;
	for (; header < &_boot_modules_headers_end; header++) {

		if (!header->size) {
			warning("ignore zero-sized boot module '",
			        Cstring((char const *)header->name), "'");
			continue;
		}
		Rom_module *rom_module = new (core_mem_alloc())
			Rom_module(_rom_module_phys(header->base), header->size,
			           (char const *)header->name);
		_rom_fs.insert(rom_module);
	}
}
