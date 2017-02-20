/*
 * \brief  Memory region types
 * \author Norman Feske
 * \author Adrian-Ken Rueegsegger
 * \author Reto Buerki
 * \date   2015-06-23
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _VIRTUALBOX__SPEC__MUEN__MEM_REGION_H_
#define _VIRTUALBOX__SPEC__MUEN__MEM_REGION_H_

/* Genode includes */
#include <base/env.h>
#include <base/lock.h>
#include <util/list.h>
#include <base/attached_io_mem_dataspace.h>
#include <rom_session/connection.h>
#include <muen/sinfo.h>

/* VirtualBox includes */
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/pdmdev.h>

struct Mem_region;

struct Mem_region : Genode::List<Mem_region>::Element,
					Genode::Attached_io_mem_dataspace
{
	typedef Genode::size_t      size_t;
	typedef Genode::addr_t      addr_t;

	PPDMDEVINS           pDevIns;
	unsigned const       iRegion;
	RTGCPHYS             vm_phys;
	PFNPGMR3PHYSHANDLER  pfnHandlerR3;
	void                *pvUserR3;
	PGMPHYSHANDLERTYPE   enmType;
	size_t				 region_size;
	bool				 _clear;

	addr_t _phys_base(Genode::Env &env, size_t size)
	{
		struct Region_info
		{
			addr_t base;
			size_t size;
		};
		static unsigned counter;
		static Region_info regions[] = {
			/* pcbios phys 0xe1000 */
			{ 0x810000000, 0x1000 },
			/* pcbios phys 0xf0000*/
			{ 0x820000000, 0x10000 },
			/* pcbios 0xffff0000 */
			{ 0x830000000, 0x10000 },
			/* VMMDev */
			{ 0x840000000, 0x400000 },
			/* VMMDev */
			{ 0x850000000, 0x4000 },
			/* vga */
			{ 0x860000000, 0x8000000 },
			/* vga phys 0xc0000 */
			{ 0x870000000, 0x9000 },
			/* acpi phys e0000 */
			{ 0x880000000, 0x1000 },
			{ 0x0, 0x0 },
		};

		Region_info cur_region;

		if (!counter) {
			Genode::Rom_connection sinfo_rom(env, "subject_info_page");
			Genode::Sinfo sinfo ((addr_t)env.rm().attach (sinfo_rom.dataspace()));

			struct Genode::Sinfo::Memregion_info region1, region4;
			if (!sinfo.get_memregion_info("vm_ram_1", &region1)) {
				Genode::error("unable to retrieve vm_ram_1 region");
				return 0;
			}
			if (!sinfo.get_memregion_info("vm_ram_4", &region4)) {
				Genode::error("unable to retrieve vm_ram_4 region");
				return 0;
			}

			cur_region.base = region1.address;
			cur_region.size = region4.address + region4.size - region1.address;
			counter++;
			_clear = false;

		} else {
			cur_region = regions[counter - 1];

			if (cur_region.size == 0)
			{
				Genode::error("region size is zero!!!");
				return 0;
			}
			counter++;
			_clear = true;
		}

		if (size > cur_region.size)
			Genode::error("size: ", Genode::Hex(size), ", "
			              "cur_region.size: ", Genode::Hex(cur_region.size));
		Assert(size <= cur_region.size);

		return cur_region.base;
	}

	size_t size() const { return region_size; }

	Mem_region(Genode::Env &env, size_t size,
	           PPDMDEVINS pDevIns, unsigned iRegion)
	:
		Attached_io_mem_dataspace(env, _phys_base(env, size), size),
		pDevIns(pDevIns),
		iRegion(iRegion),
		vm_phys(0), pfnHandlerR3(0), pvUserR3(0),
		region_size(size)
	{
		if (_clear)
			Genode::memset(local_addr<char>(), 0, size);
	}
};

#endif /* _VIRTUALBOX__SPEC__MUEN__MEM_REGION_H_ */
