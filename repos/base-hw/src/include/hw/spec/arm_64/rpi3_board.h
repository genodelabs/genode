/*
 * \brief  Board definitions for Raspberry Pi 3
 * \author Stefan Kalkowski
 * \date   2019-05-10
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__INCLUDE__HW__SPEC__ARM__RPI3__BOARD_H_
#define _SRC__INCLUDE__HW__SPEC__ARM__RPI3__BOARD_H_

#include <drivers/uart/bcm2835_mini.h>
#include <hw/spec/arm/boot_info.h>

namespace Hw::Rpi3_board {
	using Serial   = Genode::Bcm2835_mini_uart;

	enum {
		RAM_BASE   = 0,
		RAM_SIZE   = 0x20000000,

		UART_BASE  = 0x3f215000,
		UART_SIZE  = 0x1000,
		UART_CLOCK = 250000000,

		IRQ_CONTROLLER_BASE = 0x3f00b000,
		IRQ_CONTROLLER_SIZE = 0x1000,

		LOCAL_IRQ_CONTROLLER_BASE = 0x40000000,
		LOCAL_IRQ_CONTROLLER_SIZE = 0x1000,
	};
};

#endif /* _SRC__INCLUDE__HW__SPEC__ARM__RPI3__BOARD_H_ */
