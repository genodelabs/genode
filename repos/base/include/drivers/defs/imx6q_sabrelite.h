/*
 * \brief  MMIO and IRQ definitions of the i.MX6Quad Sabrelite
 * \author Stefan Kalkowski
 * \date   2019-01-05
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DRIVERS__DEFS__IMX6Q_SABRELITE_H_
#define _INCLUDE__DRIVERS__DEFS__IMX6Q_SABRELITE_H_

/* Genode includes */
#include <drivers/defs/imx6.h>

namespace Imx6q_sabrelite {

	using namespace Imx6;

	enum {
		RAM_BASE = 0x10000000,
		RAM_SIZE = 0x40000000,
	};
};

#endif /* _INCLUDE__DRIVERS__DEFS__IMX6Q_SABRELITE_H_ */
