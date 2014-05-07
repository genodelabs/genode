/*
 * \brief  Serial output driver specific for the i.MX31
 * \author Norman Feske
 * \date   2012-08-30
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PLATFORM__IMX31__DRIVERS__SERIAL_LOG_H_
#define _INCLUDE__PLATFORM__IMX31__DRIVERS__SERIAL_LOG_H_

/* Genode includes */
#include <board.h>
#include <drivers/uart/imx31_uart_base.h>

namespace Genode
{
	struct Serial_log : Imx31_uart_base
	{
		/**
		 * Constructor
		 *
		 * \param baud_rate  targeted transfer baud-rate
		 *
		 * XXX: The 'baud_rate' argument is ignored for now.
		 */
		Serial_log(unsigned const baud_rate)
		: Imx31_uart_base(Board::UART_1_MMIO_BASE) { }
	};
}

#endif /* _INCLUDE__PLATFORM__IMX31__DRIVERS__SERIAL_LOG_H_ */

