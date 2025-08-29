/*
 * \brief  Generic Interrupt Controller version 3
 * \author Sebastian Sumpf
 * \date   2019-06-27
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <board.h>
#include <platform.h>

using namespace Board;
using Platform = Core::Platform;

static inline Genode::addr_t redistributor_addr()
{
	return Platform::mmio_to_virt(Cpu_mmio::IRQ_CONTROLLER_REDIST_BASE
	                              + (Cpu::executing_id().value * 0x20000));
};


Hw::Pic::Pic()
:
	_distr({(char *)Platform::mmio_to_virt(Cpu_mmio::IRQ_CONTROLLER_DISTR_BASE),
	                                       Cpu_mmio::IRQ_CONTROLLER_DISTR_SIZE}),
	_redistr({(char *)redistributor_addr(), Cpu_mmio::IRQ_CONTROLLER_REDIST_SIZE}),
	_redistr_sgi({(char *)redistributor_addr() + 0x10000,
	             Cpu_mmio::IRQ_CONTROLLER_REDIST_SIZE - 0x10000}),
	_max_irq(_distr.max_irq())
{
	_redistributor_init();
	_cpui.init();
}
