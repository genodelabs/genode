/*
 * \brief  Base driver for the Zynq (QEMU)
 * \author Johannes Schlatow
 * \date   2015-06-30
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DRIVERS__DEFS__ZYNQ_QEMU_H_
#define _INCLUDE__DRIVERS__DEFS__ZYNQ_QEMU_H_

#include <drivers/defs/zynq.h>

namespace Zynq_qemu {

	using namespace Zynq;

	enum {
		RAM_0_SIZE = 0x40000000, /* 1GiB */

		CORTEX_A9_PRIVATE_TIMER_CLK = 100000000,
		CORTEX_A9_PRIVATE_TIMER_DIV = 100,
	};
};

#endif /* _INCLUDE__DRIVERS__DEFS__ZYNQ_QEMU_H_ */
