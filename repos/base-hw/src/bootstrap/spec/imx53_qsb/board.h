/*
 * \brief   i.MX53 Quickstart board definitions
 * \author  Stefan Kalkowski
 * \date    2017-03-22
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__BOOTSTRAP__SPEC__IMX53_QSB__BOARD_H_
#define _SRC__BOOTSTRAP__SPEC__IMX53_QSB__BOARD_H_

#include <hw/spec/arm/imx53_qsb_board.h>
#include <hw/spec/arm/imx_tzic.h>

#include <spec/arm/cortex_a8_page_table.h>
#include <spec/arm/cpu.h>

namespace Board {
	using namespace Hw::Imx53_qsb_board;

	using Hw::Pic;

	bool secure_irq(unsigned irq);
}

#endif /* _SRC__BOOTSTRAP__SPEC__IMX53_QSB__BOARD_H_ */
