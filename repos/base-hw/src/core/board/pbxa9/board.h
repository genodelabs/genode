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

#include <hw/spec/arm/gicv2.h>
#include <hw/spec/arm/pbxa9_board.h>
#include <spec/arm/cortex_a9_private_timer.h>

namespace Board {
	using namespace Hw::Pbxa9_board;

	using Pic = Hw::Gicv2;

	L2_cache & l2_cache();
}

#endif /* _CORE__SPEC__PBXA9__BOARD_H_ */
