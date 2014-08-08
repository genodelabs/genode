/*
 * \brief  Programmable interrupt controller for core
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PIC_H_
#define _PIC_H_

/* core includes */
#include <spec/arm_gic/pic_support.h>
#include <cpu.h>

namespace Genode
{
	/**
	 * Programmable interrupt controller for core
	 */
	class Pic;
}

class Genode::Pic : public Arm_gic
{
	public:

		/**
		 * Constructor
		 */
		Pic() : Arm_gic(Cpu::PL390_DISTRIBUTOR_MMIO_BASE,
		                Cpu::PL390_CPU_MMIO_BASE)
		{
			/* configure every shared peripheral interrupt */
			for (unsigned i = min_spi; i <= _max_interrupt; i++) {
				_distr.write<Distr::Icfgr::Edge_triggered>(0, i);
				_distr.write<Distr::Ipriorityr::Priority>(0, i);
				_distr.write<Distr::Itargetsr::Cpu_targets>(0xff, i);
			}

			/* disable the priority filter */
			_cpui.write<Cpui::Pmr::Priority>(0xff);

			/* signal secure IRQ via FIQ interface */
			Cpui::Ctlr::access_t ctlr = 0;
			Cpui::Ctlr::Enable_grp0::set(ctlr, 1);
			Cpui::Ctlr::Enable_grp1::set(ctlr, 1);
			Cpui::Ctlr::Fiq_en::set(ctlr, 1);
			_cpui.write<Cpui::Ctlr>(ctlr);

			/* use whole band of prios */
			_cpui.write<Cpui::Bpr::Binary_point>(~0);

			/* enable device */
			_distr.write<Distr::Ctlr>(Distr::Ctlr::Enable::bits(1));
		}

		/**
		 * Mark interrupt 'i' unsecure
		 */
		void unsecure(unsigned const i) {
			_distr.write<Distr::Igroupr::Group_status>(1, i); }
};


bool Genode::Arm_gic::_use_security_ext() { return 1; }


namespace Kernel { class Pic : public Genode::Pic { }; }

#endif /* _PIC_H_ */
