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

namespace Genode { struct Board_base; }


/**
 * Board driver base
 */
struct Genode::Board_base : Exynos5
{
	enum
	{
		/* UART */
		UART_2_CLOCK = 62668800,

		/* wether board provides security extension */
		SECURITY_EXTENSION = 0,
	};
};

#endif /* _INCLUDE__DRIVERS__BOARD_BASE_H_ */
