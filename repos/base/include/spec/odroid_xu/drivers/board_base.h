/*
 * \brief  Driver base for the Odroid XU board
 * \author Stefan Kalkowski
 * \date   2013-11-25
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__ODROID_XU__DRIVERS__BOARD_BASE_H_
#define _INCLUDE__SPEC__ODROID_XU__DRIVERS__BOARD_BASE_H_

/* Genode includes */
#include <spec/exynos5/board_base.h>

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

#endif /* _INCLUDE__SPEC__ODROID_XU__DRIVERS__BOARD_BASE_H_ */
