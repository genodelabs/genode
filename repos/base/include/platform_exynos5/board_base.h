/*
 * \brief  Board-driver base
 * \author Stefan Kalkowski
 * \date   2013-11-25
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _EXYNOS5__BOARD_BASE_H_
#define _EXYNOS5__BOARD_BASE_H_

namespace Genode
{
	/**
	 * Board-driver base
	 */
	class Exynos5;
}

class Genode::Exynos5
{
	public:

		enum {
			/* normal RAM */
			RAM_0_BASE = 0x40000000,
			RAM_0_SIZE = 0x80000000,

			/* device IO memory */
			MMIO_0_BASE = 0x10000000,
			MMIO_0_SIZE = 0x10000000,

			/* pulse-width-modulation timer  */
			PWM_MMIO_BASE = 0x12dd0000,
			PWM_MMIO_SIZE = 0x1000,
			PWM_CLOCK     = 66000000,
			PWM_IRQ_0     = 68,

			/* multicore timer */
			MCT_MMIO_BASE = 0x101c0000,
			MCT_MMIO_SIZE = 0x1000,
			MCT_CLOCK     = 24000000,
			MCT_IRQ_L0    = 152,
			MCT_IRQ_L1    = 153,

			/* CPU cache */
			CACHE_LINE_SIZE_LOG2 = 6,
		};
};

#endif /* _EXYNOS5__BOARD_BASE_H_ */
