/*
 * \brief  Serial output driver for console lib
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2013-01-09
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__EXYNOS_UART__DRIVERS__SERIAL_LOG_H_
#define _INCLUDE__EXYNOS_UART__DRIVERS__SERIAL_LOG_H_

/* Genode includes */
#include <board.h>
#include <drivers/uart/exynos_uart_base.h>

namespace Genode
{
	/**
	 * Serial output driver for console lib
	 */
	class Serial_log : public Exynos_uart_base
	{
		public:

			/**
			 * Constructor
			 *
			 * \param baud_rate  targeted transfer baud-rate
			 */
			Serial_log(unsigned const baud_rate) :
				Exynos_uart_base(Board::UART_2_MMIO_BASE,
				                 Board::UART_2_CLOCK, baud_rate)
			{ }
	};
}

#endif /* _INCLUDE__EXYNOS_UART__DRIVERS__SERIAL_LOG_H_ */

