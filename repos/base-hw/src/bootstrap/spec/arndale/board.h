/*
 * \brief   Arndale specific board definitions
 * \author  Stefan Kalkowski
 * \date    2017-04-03
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__BOOTSTRAP__SPEC__ARNDALE__BOARD_H_
#define _SRC__BOOTSTRAP__SPEC__ARNDALE__BOARD_H_

#include <hw/spec/arm/arndale_board.h>
#include <hw/spec/arm/lpae.h>
#include <spec/arm/cpu.h>
#include <hw/spec/arm/gicv2.h>

namespace Board {
	using namespace Hw::Arndale_board;
	using Pic = Hw::Gicv2;
	static constexpr bool NON_SECURE = true;
}

#endif /* _SRC__BOOTSTRAP__SPEC__ARNDALE__BOARD_H_ */
