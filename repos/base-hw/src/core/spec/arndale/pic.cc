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
#include <pic.h>
#include <platform.h>

using namespace Genode;

void Pic::_init()
{
	/* disable device */
	_distr.write<Distr::Ctlr>(0);

	/* configure every shared peripheral interrupt */
	for (unsigned i = min_spi; i <= _max_irq; i++) {
		/* mark as non-secure */
		_distr.write<Distr::Igroupr::Group_status>(1, i);
		_distr.write<Distr::Icfgr::Edge_triggered>(0, i);
		_distr.write<Distr::Ipriorityr::Priority>(0, i);
		_distr.write<Distr::Icenabler::Clear_enable>(1, i);
	}
	/* enable device */
	Distr::Ctlr::access_t v = 0;
	Distr::Ctlr::Enable_grp0::set(v, 1);
	Distr::Ctlr::Enable_grp1::set(v, 1);
	_distr.write<Distr::Ctlr>(v);
}


void Pic::init_cpu_local()
{
	_cpui.write<Cpui::Ctlr>(0);

	/* mark software-generated IRQs as being non-secure */
	for (unsigned i = 0; i < min_spi; i++)
		_distr.write<Distr::Igroupr::Group_status>(1, i);

	/* disable the priority filter */
	_cpui.write<Cpui::Pmr::Priority>(_distr.min_priority());

	/* disable preemption of IRQ handling by other IRQs */
	_cpui.write<Cpui::Bpr::Binary_point>(~0);

	/* enable device */
	Cpui::Ctlr::access_t v = 0;
	Cpui::Ctlr::Enable_grp0::set(v, 1);
	Cpui::Ctlr::Enable_grp1::set(v, 1);
	Cpui::Ctlr::Fiq_en::set(v, 1);
	_cpui.write<Cpui::Ctlr>(v);
}


Pic::Pic()
: _distr(Platform::mmio_to_virt(Board::IRQ_CONTROLLER_DISTR_BASE)),
  _cpui (Platform::mmio_to_virt(Board::IRQ_CONTROLLER_CPU_BASE)),
  _last_iar(Cpui::Iar::Irq_id::bits(spurious_id)),
  _max_irq(_distr.max_irq())
{
	_init();
}
