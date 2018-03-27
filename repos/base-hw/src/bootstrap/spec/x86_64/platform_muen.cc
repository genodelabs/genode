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

#include <platform.h>
#include <muen/sinfo.h>

using Sinfo = Genode::Sinfo;

enum {
	TIMER_BASE_ADDR         = 0xe00010000,
	TIMER_SIZE              = 0x1000,
	TIMER_PREEMPT_BASE_ADDR = 0xe00011000,
	TIMER_PREEMPT_SIZE      = 0x1000,

	COM1_PORT               = 0x3f8,
};


Bootstrap::Platform::Board::Board()
: core_mmio(Memory_region { Sinfo::PHYSICAL_BASE_ADDR, Sinfo::SIZE },
            Memory_region { TIMER_BASE_ADDR, TIMER_SIZE },
            Memory_region { TIMER_PREEMPT_BASE_ADDR, TIMER_PREEMPT_SIZE })
{
	Sinfo sinfo(Sinfo::PHYSICAL_BASE_ADDR);
	const struct Sinfo::Resource_type *
		region = sinfo.get_resource("ram", Sinfo::RES_MEMORY);

	if (!region)
		Genode::error("Unable to retrieve base-hw ram region");
	else
		early_ram_regions.add(Memory_region
				{ region->data.mem.address, region->data.mem.size });
}


unsigned Bootstrap::Platform::enable_mmu()
{
	Cpu::Cr3::write(Cpu::Cr3::Pdb::masked((addr_t)core_pd->table_base));
	return 0;
}


Board::Serial::Serial(Genode::addr_t, Genode::size_t, unsigned baudrate)
:X86_uart(COM1_PORT, 0, baudrate) {}
