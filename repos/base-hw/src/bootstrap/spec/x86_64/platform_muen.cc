/*
 * \brief   Platform implementations specific for x86_64
 * \author  Reto Buerki
 * \author  Stefan Kalkowski
 * \date    2015-05-04
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <cpu.h>
#include <platform.h>
#include <muen/sinfo.h>

using namespace Genode;


Platform::Board::Board()
: core_mmio(Memory_region { Sinfo::PHYSICAL_BASE_ADDR, Sinfo::SIZE },
            Memory_region { TIMER_BASE_ADDR, TIMER_SIZE },
            Memory_region { TIMER_PREEMPT_BASE_ADDR, TIMER_PREEMPT_SIZE })
{
	struct Sinfo::Memregion_info region;

	Sinfo sinfo(Sinfo::PHYSICAL_BASE_ADDR);
	if (!sinfo.get_memregion_info("ram", &region))
		error("Unable to retrieve base-hw ram region");
	else
		early_ram_regions.add(Memory_region { region.address, region.size });
}


void Platform::enable_mmu() {
	Cpu::Cr3::write(Cpu::Cr3::init((addr_t)core_pd->table_base)); }
