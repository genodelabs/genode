/*
 * \brief  Programmable interrupt controller for core
 * \author Stefan Kalkowski
 * \date   2012-10-24
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <pic.h>

using namespace Genode;

void Pic::_init()
{
	/* disable device */
	_distr.write<Distr::Ctlr::Enable>(0);

	/* configure every shared peripheral interrupt */
	for (unsigned i = min_spi; i <= _max_irq; i++) {
		_distr.write<Distr::Icfgr::Edge_triggered>(0, i);
		_distr.write<Distr::Ipriorityr::Priority>(0, i);
		_distr.write<Distr::Icenabler::Clear_enable>(1, i);
	}
	/* enable device */
	_distr.write<Distr::Ctlr::Enable>(1);
}


void Pic::init_cpu_local()
{
	/* disable the priority filter */
	_cpui.write<Cpui::Pmr::Priority>(_distr.min_priority());

	/* disable preemption of IRQ handling by other IRQs */
	_cpui.write<Cpui::Bpr::Binary_point>(~0);

	/* enable device */
	_cpui.write<Cpui::Ctlr::Enable>(1);
}
