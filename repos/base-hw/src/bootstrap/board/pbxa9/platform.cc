/*
 * \brief   Pbxa9 specific platform implementation
 * \author  Stefan Kalkowski
 * \date    2016-10-20
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* bootstrap includes */
#include <platform.h>

using namespace Board;

Bootstrap::Platform::Board::Board()
:
	early_ram_regions(Memory_region { RAM_0_BASE, RAM_0_SIZE },
	                  Memory_region { RAM_1_BASE, RAM_1_SIZE }),
	core_mmio(Memory_region { CORTEX_A9_PRIVATE_MEM_BASE,
	                          CORTEX_A9_PRIVATE_MEM_SIZE },
	          Memory_region { PL011_0_MMIO_BASE,
	                          PL011_0_MMIO_SIZE },
	          Memory_region { PL310_MMIO_BASE,
	                          PL310_MMIO_SIZE })
{ }


bool Board::Cpu::errata(Board::Cpu::Errata) { return false; }


void Board::Cpu::wake_up_all_cpus(void * const ip)
{
	/**
	 * set the entrypoint for the other CPUs via the flags register
	 * of the system control registers. ARMs boot monitor code will
	 * read out this register and jump to it after the cpu received
	 * an interrupt
	 */
	struct System_control : Genode::Mmio<0x38>
	{
		struct Flagsset : Register<0x30, 32> { };
		struct Flagsclr : Register<0x34, 32> { };

		System_control(void * const ip) : Mmio({(char *)SYSTEM_CONTROL_MMIO_BASE, Mmio::SIZE})
		{
			write<Flagsclr>(~0UL);
			write<Flagsset>(reinterpret_cast<Flagsset::access_t>(ip));
		}
	} sc(ip);
}
