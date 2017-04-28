/*
 * \brief  Board driver
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2012-10-24
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__IMX53_QSB__BOARD_H_
#define _CORE__SPEC__IMX53_QSB__BOARD_H_

#include <drivers/defs/imx53_qsb.h>

namespace Board {
	using namespace Imx53_qsb;
	static constexpr bool SMP = false;
}

#endif /* _CORE__SPEC__IMX53_QSB__BOARD_H_ */
