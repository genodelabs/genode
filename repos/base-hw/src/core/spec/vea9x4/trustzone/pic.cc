/*
 * \brief  Programmable interrupt controller for core
 * \author Martin stein
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
	for (unsigned i=MIN_SPI; i <= _max_interrupt; i++) {
		_distr.write<Distr::Icfgr::Edge_triggered>(0, i);
		_distr.write<Distr::Ipriorityr::Priority>(0, i);
		_distr.write<Distr::Itargetsr::Cpu_targets>(0xff, i);
	}

	/* disable the priority filter */
	_cpui.write<Cpui::Pmr::Priority>(0xff);

	/* signal secure IRQ via FIQ interface */
	_cpui.write<Cpui::Ctlr>(Cpui::Ctlr::Enable_grp0::bits(1)  |
	                        Cpui::Ctlr::Enable_grp1::bits(1) |
	                        Cpui::Ctlr::Fiq_en::bits(1));

	/* use whole band of prios */
	_cpui.write<Cpui::Bpr::Binary_point>(Cpui::Bpr::NO_PREEMPTION);

	/* enable device */
	_distr.write<Distr::Ctlr>(Distr::Ctlr::Enable::bits(1));
}

void Pic::unsecure(unsigned const i) {
	_distr.write<Distr::Igroupr::Group_status>(1, i); }
