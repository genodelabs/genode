/*
 * \brief  Board driver
 * \author Stefan Kalkowski
 * \date   2017-10-18
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__NIT6_SOLOX__BOARD_H_
#define _CORE__SPEC__NIT6_SOLOX__BOARD_H_

#include <hw/spec/arm/gicv2.h>
#include <hw/spec/arm/nit6_solox_board.h>
#include <spec/arm/cortex_a9_private_timer.h>

namespace Board {
	using namespace Hw::Nit6_solox_board;

	using Pic      = Hw::Gicv2;
	using L2_cache = Hw::Pl310;

	L2_cache & l2_cache();

	enum {
		CORTEX_A9_PRIVATE_TIMER_CLK = 500000000, /* timer clk runs half the CPU freq */
		CORTEX_A9_PRIVATE_TIMER_DIV = 100,
	};
}

#endif /* _CORE__SPEC__NIT6_SOLOX__BOARD_H_ */
