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

#ifndef _PIC__CORELINK_GIC400_H_
#define _PIC__CORELINK_GIC400_H_

/* core includes */
#include <pic/arm_gic.h>

namespace Corelink_gic400
{
	using namespace Genode;

	/**
	 * Programmable interrupt controller for core
	 *
	 * CoreLink GIC-400 Revision r0p0
	 */
	class Pic : public Arm_gic::Pic
	{
		enum {
			DISTR_OFFSET = 0x1000,
			CPU_OFFSET   = 0x2000,
		};

		public:

			/**
			 * Constructor
			 */
			Pic(addr_t const base) : Arm_gic::Pic(base + DISTR_OFFSET,
			                                      base + CPU_OFFSET) { }
	};
}

#endif /* _PIC__CORELINK_GIC400_H_ */

