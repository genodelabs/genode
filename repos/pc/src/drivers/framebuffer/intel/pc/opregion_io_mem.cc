/*
 * \brief  Linux emulation environment specific to this driver - Intel opregion
 * \author Alexander Boettcher
 * \date   2022-01-21
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */


#include <base/log.h>
#include <lx_kit/env.h>
#include <base/attached_io_mem_dataspace.h>
#include <base/attached_rom_dataspace.h>

#include <region_map/client.h>
#include <rm_session/connection.h>
#include <rom_session/connection.h>


extern "C" void * intel_io_mem_map(unsigned long const phys,
                                   unsigned long const size)
{
	using namespace Genode;

	static Constructible<Attached_rom_dataspace> rom_opregion { };
	static addr_t opregion_start = 0;
	static addr_t opregion_size  = 0;

	if (!rom_opregion.constructed()) {
		try {
			rom_opregion.construct(Lx_kit::env().env, "intel_opregion");

			Dataspace_client ds_client(rom_opregion->cap());

			auto mem_local = rom_opregion->local_addr<char>();
			opregion_start = *(addr_t*)(mem_local + ds_client.size() - sizeof(addr_t) * 2);
			opregion_size  = *(addr_t*)(mem_local + ds_client.size() - sizeof(addr_t));

			if (opregion_size > ds_client.size())
				opregion_size = ds_client.size();
		} catch (...) {
			error("Intel opregion ROM lookup failed");
			if (rom_opregion.constructed())
				rom_opregion.destruct();
			return nullptr;
		}
	}

	if ((opregion_start <= phys) &&
	    (phys + size <= opregion_start + opregion_size)) {

		try {
			auto ptr = ((addr_t)rom_opregion->local_addr<void>())
			           + (phys - opregion_start) + (phys & 0xffful);
			return (void *)ptr;
		} catch (...) {
			error("Intel opregion lookup failed");
			return nullptr;
		}
	}

	warning("Unknown memremap range ", Hex_range(phys, size));

	return nullptr;
}
