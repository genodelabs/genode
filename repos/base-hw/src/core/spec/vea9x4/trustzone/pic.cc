/*
 * \brief  Programmable interrupt controller for core
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2011-10-26
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <pic.h>

using namespace Genode;

void Arm_gic::_init()
{
	/* configure every shared peripheral interrupt */
	for (unsigned i = min_spi; i <= _max_irq; i++) {
		_distr.write<Distr::Icfgr::Edge_triggered>(0, i);
		_distr.write<Distr::Ipriorityr::Priority>(0, i);
		_distr.write<Distr::Itargetsr::Cpu_targets>(~0, i);
	}

	/* disable the priority filter */
	_cpui.write<Cpui::Pmr::Priority>(~0);

	/* signal secure IRQ via FIQ interface */
	typedef Cpui::Ctlr Ctlr;
	Ctlr::access_t v = 0;
	Ctlr::Enable_grp0::set(v, 1);
	Ctlr::Enable_grp1::set(v, 1);
	Ctlr::Fiq_en::set(v, 1);
	_cpui.write<Ctlr>(v);

	/* use whole band of prios */
	_cpui.write<Cpui::Bpr::Binary_point>(~0);

	/* enable device */
	_distr.write<Distr::Ctlr>(Distr::Ctlr::Enable::bits(1));
}

void Pic::unsecure(unsigned const i) {
	_distr.write<Distr::Igroupr::Group_status>(1, i); }
