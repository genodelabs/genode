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

Hw::Gicv2::Gicv2()
: _distr(Board::Cpu_mmio::IRQ_CONTROLLER_DISTR_BASE),
  _cpui (Board::Cpu_mmio::IRQ_CONTROLLER_CPU_BASE),
  _last_iar(Cpu_interface::Iar::Irq_id::bits(spurious_id)),
  _max_irq(_distr.max_irq())
{
	static bool distributor_initialized = false;
	bool use_group_1 = Board::NON_SECURE &&
	                   _distr.read<Distributor::Typer::Security_extension>();

	if (!distributor_initialized) {
		distributor_initialized = true;

		/* disable device */
		_distr.write<Distributor::Ctlr>(0);

		/* configure every shared peripheral interrupt */
		for (unsigned i = min_spi; i <= _max_irq; i++) {
			if (use_group_1) {
				_distr.write<Distributor::Igroupr::Group_status>(1, i);
			}
			_distr.write<Distributor::Icfgr::Edge_triggered>(0, i);
			_distr.write<Distributor::Ipriorityr::Priority>(0, i);
			_distr.write<Distributor::Icenabler::Clear_enable>(1, i);
		}

		/* enable device */
		Distributor::Ctlr::access_t v = 0;
		if (use_group_1) {
			Distributor::Ctlr::Enable_grp0::set(v, 1);
			Distributor::Ctlr::Enable_grp1::set(v, 1);
		} else {
			Distributor::Ctlr::Enable::set(v, 1);
		}
		_distr.write<Distributor::Ctlr>(v);
	}

	if (use_group_1) {
		_cpui.write<Cpu_interface::Ctlr>(0);

		/* mark software-generated IRQs as being non-secure */
		for (unsigned i = 0; i < min_spi; i++)
			_distr.write<Distributor::Igroupr::Group_status>(1, i);
	}

	/* disable the priority filter */
	_cpui.write<Cpu_interface::Pmr::Priority>(_distr.min_priority());

	/* disable preemption of IRQ handling by other IRQs */
	_cpui.write<Cpu_interface::Bpr::Binary_point>(~0);

	/* enable device */
	Cpu_interface::Ctlr::access_t v = 0;
	if (use_group_1) {
		Cpu_interface::Ctlr::Enable_grp0::set(v, 1);
		Cpu_interface::Ctlr::Enable_grp1::set(v, 1);
		Cpu_interface::Ctlr::Fiq_en::set(v, 1);
	} else {
		Cpu_interface::Ctlr::Enable::set(v, 1);
	}
	_cpui.write<Cpu_interface::Ctlr>(v);
}
