/*
 * \brief  Serial output driver for core
 * \author Johannes Schlatow
 * \date   2014-12-15
 */

/*
 * Copyright (C) 2014-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__SPEC__XILINX__SERIAL_H_
#define _CORE__INCLUDE__SPEC__XILINX__SERIAL_H_

/* core includes */
#include <board.h>

/* Genode includes */
#include <drivers/uart_base.h>

namespace Genode { class Serial; }

/**
 * Serial output driver for core
 */
class Genode::Serial : public Xilinx_uartps_base
{
	public:

		/**
		 * Constructor
		 *
		 * \param baud_rate  targeted transfer baud-rate
		 */
		Serial(unsigned const baud_rate)
		:
			Xilinx_uartps_base(Board::UART_0_MMIO_BASE,
			                   Board::UART_CLOCK, baud_rate)
		{ }
};

#endif /* _CORE__INCLUDE__SPEC__XILINX__SERIAL_H_ */
