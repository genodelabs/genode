/*
 * \brief  Driver base for Odroid-x2 board
 * \author Alexy Gallardo Segura <alexy@uclv.cu>
 * \author Humberto López León <humberto@uclv.cu>
 * \author Reinier Millo Sánchez <rmillo@uclv.cu>
 * \date   2015-04-27
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DRIVERS__BOARD_BASE_H_
#define _INCLUDE__DRIVERS__BOARD_BASE_H_

/* Genode includes */
#include <platform_exynos4/board_base.h>

namespace Genode { struct Board_base; }


/**
 * Board driver base
 */
struct Genode::Board_base : Exynos4
{
	enum
	{
		/* clock management unit */
		CMU_MMIO_BASE = 0x10040000,
		CMU_MMIO_SIZE = 0x18000,

		/* power management unit */
		PMU_MMIO_BASE = 0x10020000,
		PMU_MMIO_SIZE = 0x5000,
	};
};

#endif /* _INCLUDE__DRIVERS__BOARD_BASE_H_ */
