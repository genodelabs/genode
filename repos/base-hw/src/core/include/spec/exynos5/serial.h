/*
 * \brief  Serial output driver for core
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2013-01-09
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SPEC__EXYNOS5__SERIAL_H_
#define _CORE__INCLUDE__SPEC__EXYNOS5__SERIAL_H_

/* core includes */
#include <board.h>
#include <platform.h>

/* Genode includes */
#include <drivers/uart_base.h>

namespace Genode
{
	/**
	 * Serial output driver for core
	 */
	class Serial : public Exynos_uart_base
	{
		public:

			/**
			 * Constructor
			 *
			 * \param baud_rate  targeted transfer baud-rate
			 */
			Serial(unsigned const baud_rate)
			:
				Exynos_uart_base(Platform::mmio_to_virt(Board::UART_2_MMIO_BASE),
				                 Board::UART_2_CLOCK, baud_rate)
			{ }
	};
}

#endif /* _CORE__INCLUDE__SPEC__EXYNOS5__SERIAL_H_ */
