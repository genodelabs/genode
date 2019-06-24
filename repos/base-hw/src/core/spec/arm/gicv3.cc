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

using namespace Genode;

Hw::Pic::Pic()
: _distr(Platform::mmio_to_virt(Board::Cpu_mmio::IRQ_CONTROLLER_DISTR_BASE)),
  _redistr(Platform::mmio_to_virt(Board::Cpu_mmio::IRQ_CONTROLLER_REDIST_BASE)),
  _redistr_sgi(Platform::mmio_to_virt(Board::Cpu_mmio::IRQ_CONTROLLER_REDIST_BASE)
               + Board::Cpu_mmio::IRQ_CONTROLLER_REDIST_SIZE / 2),
  _max_irq(_distr.max_irq())
{
	_redistributor_init();
	_cpui.init();
}
