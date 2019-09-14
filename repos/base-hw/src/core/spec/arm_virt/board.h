/*
 * \brief  Arm virt board driver for core
 * \author Piotr Tworek
 * \date   2019-09-15
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

#include <hw/spec/arm/arm_virt_board.h>
#include <hw/spec/arm/gicv2.h>
#include <spec/arm/generic_timer.h>

namespace Board {
	using namespace Hw::Arm_virt_board;
	using Pic = Hw::Gicv2;

	enum { TIMER_IRQ = 30 /* PPI IRQ 14 */ };
};

