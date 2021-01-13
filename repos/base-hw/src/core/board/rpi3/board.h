/*
 * \brief  Board driver for core
 * \author Stefan Kalkowski
 * \date   2019-05-10
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__RPI3__BOARD_H_
#define _CORE__SPEC__RPI3__BOARD_H_

#include <hw/spec/arm_64/rpi3_board.h>
#include <spec/arm/bcm2837_pic.h>
#include <spec/arm/generic_timer.h>

namespace Board {
	using namespace Hw::Rpi3_board;

	enum { TIMER_IRQ = 1 };
};

#endif /* _CORE__SPEC__RPI3__BOARD_H_ */
