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

/* base-hw internal includes */
#include <hw/spec/arm/gicv2.h>
#include <hw/spec/arm/nit6_solox_board.h>

/* base-hw core includes */
#include <spec/arm/cortex_a9_global_timer.h>
#include <spec/cortex_a9/cpu.h>

namespace Board {

	using namespace Hw::Nit6_solox_board;

	class Global_interrupt_controller { public: void init() {} };
	class Pic : public Hw::Gicv2 { public: Pic(Global_interrupt_controller &) { } };

	using L2_cache = Hw::Pl310;

	L2_cache & l2_cache();

	enum {
		CORTEX_A9_GLOBAL_TIMER_CLK = 500000000, /* timer clk runs half the CPU freq */
		CORTEX_A9_GLOBAL_TIMER_DIV = 100,
	};
}

#endif /* _CORE__SPEC__NIT6_SOLOX__BOARD_H_ */
