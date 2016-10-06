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

#include <base/internal/unmanaged_singleton.h>

using namespace Genode;


Memory_region_array & Platform::ram_regions()
{
	return *unmanaged_singleton<Memory_region_array>(
		Memory_region { 0, 128 * 1024 * 1024 } );
}


Cpu::User_context::User_context() { }

Memory_region_array & Platform::core_mmio_regions() {
	return *unmanaged_singleton<Memory_region_array>(); }

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
