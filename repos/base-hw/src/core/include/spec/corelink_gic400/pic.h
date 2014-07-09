/*
 * \brief  Programmable interrupt controller for core
 * \author Martin stein
 * \date   2013-01-22
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PIC_H_
#define _PIC_H_

/* core includes */
#include <spec/arm_gic/pic_support.h>
#include <board.h>

namespace Genode
{
	/**
	 * Programmable interrupt controller for core
	 */
	class Pic;
}

class Genode::Pic : public Arm_gic
{
	private:

		enum {
			BASE       = Board::GIC_CPU_MMIO_BASE,
			DISTR_BASE = BASE + 0x1000,
			CPUI_BASE  = BASE + 0x2000,
		};

	public:

		/**
		 * Constructor
		 */
		Pic() : Arm_gic(DISTR_BASE, CPUI_BASE) { }
};

namespace Kernel { class Pic : public Genode::Pic { }; }

#endif /* _PIC_H_ */
