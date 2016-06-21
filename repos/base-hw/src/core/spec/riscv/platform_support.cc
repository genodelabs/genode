/*
 * \brief   Platform implementations specific for RISC-V
 * \author  Sebastian Sumpf
 * \date    2015-06-02
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <platform.h>
#include <board.h>
#include <cpu.h>

using namespace Genode;


Native_region * Platform::_ram_regions(unsigned const i)
{
	static Native_region _regions[] = { { 0, 128 * 1024 * 1024 } };
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Cpu::User_context::User_context() { }

Native_region * Platform::_core_only_mmio_regions(unsigned) { return 0; }

void Platform::_init_io_port_alloc() { }

void Platform::_init_io_mem_alloc() { }

void Platform::_init_additional() { }

void Platform::setup_irq_mode(unsigned, unsigned, unsigned) { }

long Platform::irq(long const user_irq) { return 0; }

bool Platform::get_msi_params(const addr_t mmconf, addr_t &address,
                              addr_t &data, unsigned &irq_number)
{
	return false;
}
