/*
 * \brief  Board driver for core
 * \author Stefan Kalkowski
 * \date   2015-02-09
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__PBXA9__BOARD_H_
#define _CORE__SPEC__PBXA9__BOARD_H_

/* base-hw internal includes */
#include <hw/spec/arm/gicv2.h>
#include <hw/spec/arm/pbxa9_board.h>

/* base-hw core includes */
#include <spec/arm/cortex_a9_global_timer.h>
#include <spec/arm/cortex_a9_cpu.h>

#include <no_vcpu_board.h>

namespace Board {

	using namespace Hw::Pbxa9_board;

	class Global_interrupt_controller { public: void init() {} };
	class Pic : public Hw::Gicv2 { public: Pic(Global_interrupt_controller &) { } };

	L2_cache & l2_cache();
}

#endif /* _CORE__SPEC__PBXA9__BOARD_H_ */
