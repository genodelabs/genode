/*
 * \brief  MMIO and IRQ definitions for the Odroid XU board
 * \author Stefan Kalkowski
 * \date   2013-11-25
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DRIVERS__DEFS__ODROID_XU_H_
#define _INCLUDE__DRIVERS__DEFS__ODROID_XU_H_

/* Genode includes */
#include <drivers/defs/exynos5.h>

namespace Odroid_xu {

	using namespace Exynos5;

	enum {
		/* UART */
		UART_2_CLOCK = 62668800,
	};
};

#endif /* _INCLUDE__DRIVERS__DEFS__ODROID_XU_H_ */
