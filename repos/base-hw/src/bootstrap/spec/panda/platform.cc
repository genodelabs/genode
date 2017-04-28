/*
 * \brief   Parts of platform that are specific to Pandaboard
 * \author  Stefan Kalkowski
 * \date    2017-01-30
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <platform.h>

using namespace Board;

Bootstrap::Platform::Board::Board()
: early_ram_regions(Memory_region { RAM_0_BASE, RAM_0_SIZE }),
  core_mmio(Memory_region { CORTEX_A9_PRIVATE_MEM_BASE,
                            CORTEX_A9_PRIVATE_MEM_SIZE },
            Memory_region { TL16C750_3_MMIO_BASE,
                            TL16C750_MMIO_SIZE },
            Memory_region { PL310_MMIO_BASE,
                            PL310_MMIO_SIZE }) { }


bool Bootstrap::Cpu::errata(Bootstrap::Cpu::Errata err) {
	return false; }


void Bootstrap::Cpu::wake_up_all_cpus(void * const ip)
{
	struct Wakeup_generator : Genode::Mmio
	{
		struct Aux_core_boot_0 : Register<0x800, 32> {
			struct Cpu1_status : Bitfield<2, 2> { }; };

		struct Aux_core_boot_1 : Register<0x804, 32> { };

		Wakeup_generator(void * const ip) : Mmio(CORTEX_A9_WUGEN_MMIO_BASE)
		{
			write<Aux_core_boot_1>((addr_t)ip);
			write<Aux_core_boot_0::Cpu1_status>(1);
		}
	};

	Wakeup_generator wgen(ip);
	asm volatile("dsb\n"
	             "sev\n");
}
