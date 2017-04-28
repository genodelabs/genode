/*
 * \brief  MMIO and IRQ definitions of the i.MX53 Quickstart board
 * \author Stefan Kalkowski
 * \date   2012-10-24
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DRIVERS__DEFS__IMX53_QSB_H_
#define _INCLUDE__DRIVERS__DEFS__IMX53_QSB_H_

/* Genode includes */
#include <drivers/defs/imx53.h>

namespace Imx53_qsb {

	using namespace Imx53;

	enum {
		RAM0_BASE = RAM_BANK_0_BASE,
		RAM0_SIZE = 0x20000000,
		RAM1_BASE = RAM_BANK_1_BASE,
		RAM1_SIZE = 0x20000000,
	};
};

#endif /* _INCLUDE__DRIVERS__DEFS__IMX53_QSB_H_ */
