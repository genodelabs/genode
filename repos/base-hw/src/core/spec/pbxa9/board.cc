/*
 * \brief   Board implementation specific to PBXA9
 * \author  Stefan Kalkowski
 * \date    2016-01-07
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <util/mmio.h>
#include <board.h>
#include <kernel/kernel.h>

bool Cortex_a9::Board::errata(Cortex_a9::Board::Errata err) {
	return false; }


void Cortex_a9::Board::wake_up_all_cpus(void * const ip)
{
	/**
	 * set the entrypoint for the other CPUs via the flags register
	 * of the system control registers. ARMs boot monitor code will
	 * read out this register and jump to it after the cpu received
	 * an interrupt
	 */
	struct System_control : Genode::Mmio
	{
		struct Flagsset : Register<0x30, 32> { };
		struct Flagsclr : Register<0x34, 32> { };

		System_control(void * const ip)
		: Mmio(SYSTEM_CONTROL_MMIO_BASE)
		{
			write<Flagsclr>(~0UL);
			write<Flagsset>(reinterpret_cast<Flagsset::access_t>(ip));
		}
	} sc(ip);

	/* send an IPI to all other cpus */
	Kernel::pic()->send_ipi();
}
