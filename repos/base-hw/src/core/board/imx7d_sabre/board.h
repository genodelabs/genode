/*
 * \brief  Board driver for core
 * \author Stefan Kalkowski
 * \date   2018-11-07
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__IMX7D_SABRE__BOARD_H_
#define _CORE__SPEC__IMX7D_SABRE__BOARD_H_

/* base-hw internal includes */
#include <hw/spec/arm/imx7d_sabre_board.h>

/* base-hw core includes */
#include <spec/arm/virtualization/gicv2.h>
#include <spec/arm/generic_timer.h>
#include <spec/arm/cpu/vcpu_state_virtualization.h>
#include <spec/arm/virtualization/board.h>
#include <spec/cortex_a15/cpu.h>

namespace Board {

	using namespace Hw::Imx7d_sabre_board;

	struct Virtual_local_pic {};

	enum { TIMER_IRQ = 30, VCPU_MAX = 16 };
}

#endif /* _CORE__SPEC__IMX7_SABRELITE__BOARD_H_ */
