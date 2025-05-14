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

	using Rom_alloc = Memory::Constrained_obj_allocator<Rom_module>;

	Rom_alloc rom_alloc { core_mem_alloc() };

	for (; header_ptr < &_boot_modules_headers_end; header_ptr++) {

		Rom_name const name((char const *)header_ptr->name);

		if (!header_ptr->size) {
			warning("ignore zero-sized boot module '", name, "'");
			continue;
		}
		rom_alloc.create(_rom_fs, name, _rom_module_phys(header_ptr->base),
		                 header_ptr->size).with_result(
			[&] (Rom_alloc::Allocation &a) { a.deallocate = false; },
			[&] (Alloc_error) { error("unable to allocate ROM meta data for ", name); });
	}
}
