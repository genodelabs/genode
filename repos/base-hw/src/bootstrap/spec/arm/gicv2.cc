/*
 * \brief  Programmable interrupt controller for core
 * \author Stefan Kalkowski
 * \date   2012-10-24
 */

/*
 * Copyright (C) 2012-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <board.h>

Hw::Global_interrupt_controller::Global_interrupt_controller()
:
	Mmio({(char *)Board::Cpu_mmio::IRQ_CONTROLLER_DISTR_BASE, Mmio::SIZE})
{
	bool use_group_1 = Board::NON_SECURE &&
	                   read<Typer::Security_extension>();

	/* disable device */
	write<Ctlr>(0);

	/* configure every shared peripheral interrupt */
	for (unsigned i = MIN_SPI; i <= max_irq(); i++) {
		if (use_group_1) {
			write<Igroupr::Group_status>(1, i);
		}
		write<Icfgr::Edge_triggered>(0, i);
		write<Ipriorityr::Priority>(0, i);
		write<Icenabler::Clear_enable>(1, i);
	}

	/* enable device */
	Ctlr::access_t v = 0;
	if (use_group_1) {
		Ctlr::Enable_grp0::set(v, 1);
		Ctlr::Enable_grp1::set(v, 1);
	} else {
		Ctlr::Enable::set(v, 1);
	}
	write<Ctlr>(v);
}


Hw::Local_interrupt_controller::Local_interrupt_controller(Distributor &distributor)
:
	Mmio({(char *)Board::Cpu_mmio::IRQ_CONTROLLER_CPU_BASE, Mmio::SIZE}),
	_distr(distributor)
{
	bool use_group_1 = Board::NON_SECURE &&
	                   _distr.read<Distributor::Typer::Security_extension>();

	if (use_group_1) {
		write<Ctlr>(0);

		/* mark software-generated IRQs being non-secure (shadowed per CPU) */
		for (unsigned i = 0; i < Distributor::MIN_SPI; i++)
			_distr.write<Distributor::Igroupr::Group_status>(1, i);
	}

	/* disable the priority filter */
	write<Pmr::Priority>(_distr.min_priority());

	/* disable preemption of IRQ handling by other IRQs */
	write<Bpr::Binary_point>(~0);

	/* enable device */
	Ctlr::access_t v = 0;
	if (use_group_1) {
		Ctlr::Enable_grp0::set(v, 1);
		Ctlr::Enable_grp1::set(v, 1);
		Ctlr::Fiq_en::set(v, 1);
	} else {
		Ctlr::Enable::set(v, 1);
	}
	write<Ctlr>(v);
}
