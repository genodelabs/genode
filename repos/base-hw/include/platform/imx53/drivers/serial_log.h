/*
 * \brief  Serial output driver specific for the i.MX53
 * \author Stefan Kalkowski
 * \date   2012-10-24
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PLATFORM__IMX53__DRIVERS__SERIAL_LOG_H_
#define _INCLUDE__PLATFORM__IMX53__DRIVERS__SERIAL_LOG_H_

/* Genode includes */
#include <drivers/board_base.h>
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
		: Imx31_uart_base(Board_base::UART_1_MMIO_BASE) { }
	};
}

#endif /* _INCLUDE__PLATFORM__IMX53__DRIVERS__SERIAL_LOG_H_ */

