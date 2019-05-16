/*
 * \brief   Raspberry PI specific board definitions
 * \author  Stefan Kalkowski
 * \date    2019-05-16
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__INCLUDE__HW__SPEC__ARM__RPI_BOARD_H_
#define _SRC__INCLUDE__HW__SPEC__ARM__RPI_BOARD_H_

#include <drivers/defs/rpi.h>
#include <drivers/uart/pl011.h>
#include <hw/spec/arm/boot_info.h>

namespace Hw::Rpi_board {
	using namespace Rpi;

	using Serial = Genode::Pl011_uart;

	enum {
		UART_BASE  = Rpi::PL011_0_MMIO_BASE,
		UART_CLOCK = Rpi::PL011_0_CLOCK,
	};
}

#endif /* _SRC__INCLUDE__HW__SPEC__ARM__RPI_BOARD_H_ */
