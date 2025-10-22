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

using namespace Core;


Hw::Global_interrupt_controller::Global_interrupt_controller()
:
	Mmio({(char*)Platform::mmio_to_virt(Board::Cpu_mmio::IRQ_CONTROLLER_DISTR_BASE),
	     Mmio::SIZE}) { }


Hw::Local_interrupt_controller::Local_interrupt_controller(Distributor &distributor)
:
	Mmio({(char*)Platform::mmio_to_virt(Board::Cpu_mmio::IRQ_CONTROLLER_CPU_BASE),
	      Mmio::SIZE}),
	_distr(distributor) { }
