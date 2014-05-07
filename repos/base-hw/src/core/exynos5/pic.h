/*
 * \brief  Interrupt controller for kernel
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _EXYNOS5__PIC_H_
#define _EXYNOS5__PIC_H_

/* core includes */
#include <pic/corelink_gic400.h>
#include <board.h>

namespace Kernel
{
	/**
	 * Interrupt controller for kernel
	 */
	class Pic : public Corelink_gic400::Pic
	{
		public:

			/**
			 * Constructor
			 */
			Pic() : Corelink_gic400::Pic(Genode::Board::GIC_CPU_MMIO_BASE) { }
	};
}


bool Arm_gic::Pic::_use_security_ext() { return 0; }


#endif /* _EXYNOS5__PIC_H_ */

