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

#ifndef _INCLUDE__SPEC__ZYNQ_QEMU__DRIVERS__BOARD_BASE_H_
#define _INCLUDE__SPEC__ZYNQ_QEMU__DRIVERS__BOARD_BASE_H_

#include <spec/zynq/drivers/board_base_support.h>

namespace Genode { struct Board_base; }

/**
 * Base driver for the Zynq platform
 */
struct Genode::Board_base : Zynq::Board_base
{
	enum {
		CORTEX_A9_PRIVATE_TIMER_CLK = 100000000,
		CORTEX_A9_PRIVATE_TIMER_DIV = 100,
	};
};

#endif /* _INCLUDE__SPEC__ZYNQ_QEMU__DRIVERS__BOARD_BASE_H_ */
