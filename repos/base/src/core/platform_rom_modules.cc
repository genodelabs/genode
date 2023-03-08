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

using namespace Core;


void Platform::_init_rom_modules()
{
	Boot_modules_header const *header_ptr = &_boot_modules_headers_begin;

	for (; header_ptr < &_boot_modules_headers_end; header_ptr++) {

		Rom_name const name((char const *)header_ptr->name);

		if (!header_ptr->size) {
			warning("ignore zero-sized boot module '", name, "'");
			continue;
		}
		new (core_mem_alloc())
			Rom_module(_rom_fs, name,
			           _rom_module_phys(header_ptr->base), header_ptr->size);
	}
}
