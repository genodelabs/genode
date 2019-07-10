/*
 * \brief  Programmable interrupt controller for core
 * \author Stefan Kalkowski
 * \date   2012-10-24
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <hw/spec/arm/gicv2.h>
#include <platform.h>

using namespace Genode;

Hw::Gicv2::Gicv2()
: _distr(Platform::mmio_to_virt(Board::Cpu_mmio::IRQ_CONTROLLER_DISTR_BASE)),
  _cpui (Platform::mmio_to_virt(Board::Cpu_mmio::IRQ_CONTROLLER_CPU_BASE)),
  _last_iar(Cpu_interface::Iar::Irq_id::bits(spurious_id)),
  _max_irq(_distr.max_irq()) { }
