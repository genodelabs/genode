/*
 * \brief   Pbxa9 specific board definitions
 * \author  Stefan Kalkowski
 * \date    2017-02-20
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__BOOTSTRAP__SPEC__PBXA9__BOARD_H_
#define _SRC__BOOTSTRAP__SPEC__PBXA9__BOARD_H_

#include <hw/spec/arm/pbxa9_board.h>

#include <spec/arm/cortex_a9_actlr.h>
#include <spec/arm/cortex_a9_page_table.h>
#include <spec/arm/cpu.h>
#include <hw/spec/arm/gicv2.h>

namespace Board {
	using namespace Hw::Pbxa9_board;

	using Pic = Hw::Gicv2;

	static constexpr bool NON_SECURE = false;
}

#endif /* _SRC__BOOTSTRAP__SPEC__PBXA9__BOARD_H_ */
