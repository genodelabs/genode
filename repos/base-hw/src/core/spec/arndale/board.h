/*
 * \brief  Board driver for core
 * \author Stefan Kalkowski
 * \date   2017-04-27
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__ARNDALE__BOARD_H_
#define _CORE__SPEC__ARNDALE__BOARD_H_

#include <hw/spec/arm/gicv2.h>
#include <hw/spec/arm/arndale_board.h>
#include <spec/arm/exynos_mct.h>

namespace Board {
	using namespace Hw::Arndale_board;

	using Pic = Hw::Gicv2;
}

#endif /* _CORE__SPEC__ARNDALE__BOARD_H_ */
