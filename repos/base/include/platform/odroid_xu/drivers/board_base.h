/*
 * \brief  Driver base for the Odroid XU board
 * \author Stefan Kalkowski
 * \date   2013-11-25
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DRIVERS__BOARD_BASE_H_
#define _INCLUDE__DRIVERS__BOARD_BASE_H_

/* Genode includes */
#include <platform_exynos5/board_base.h>

namespace Genode
{
	/**
	 * Board driver base
	 */
	struct Board_base : Exynos5
	{
		enum
		{
			/* interrupt controller */
			GIC_CPU_MMIO_BASE = 0x10481000,
			GIC_CPU_MMIO_SIZE = 0x00010000,

			/* UART */
			UART_2_MMIO_BASE = 0x12C20000,
			UART_2_CLOCK     = 62668800,
			UART_2_IRQ       = 85,

			/* CPU cache */
			CACHE_LINE_SIZE_LOG2 = 6,

			/* wether board provides security extension */
			SECURITY_EXTENSION = 0,
		};
	};
}

#endif /* _INCLUDE__DRIVERS__BOARD_BASE_H_ */
