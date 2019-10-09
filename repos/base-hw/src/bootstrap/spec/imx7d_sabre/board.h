/*
 * \brief   Imx7 Sabrelite specific board definitions
 * \author  Stefan Kalkowski
 * \date    2018-11-07
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__BOOTSTRAP__SPEC__IMX7_SABRELITE__BOARD_H_
#define _SRC__BOOTSTRAP__SPEC__IMX7_SABRELITE__BOARD_H_

#include <hw/spec/arm/imx7d_sabre_board.h>
#include <hw/spec/arm/lpae.h>
#include <spec/arm/cpu.h>
#include <hw/spec/arm/gicv2.h>

namespace Board {
	using namespace Hw::Imx7d_sabre_board;
	using Pic = Hw::Gicv2;
	static constexpr bool NON_SECURE = true;
}

#endif /* _SRC__BOOTSTRAP__SPEC__IMX&_SABRELITE__BOARD_H_ */
