/*
 * \brief  Serial output driver for core
 * \author Stefan Kalkowski
 * \date   2012-10-24
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__SPEC__IMX__SERIAL_H_
#define _CORE__INCLUDE__SPEC__IMX__SERIAL_H_

/* core includes */
#include <board.h>

/* Genode includes */
#include <drivers/uart_base.h>

namespace Genode
{
	/**
	 * Serial output driver for core
	 */
	class Serial : public Imx_uart_base
	{
		public:

			/**
			 * Constructor
			 *
			 * XXX: The 'baud_rate' argument is ignored for now.
			 */
			Serial(unsigned) : Imx_uart_base(Board::UART_1_MMIO_BASE) { }
	};
}

#endif /* _CORE__INCLUDE__SPEC__IMX__SERIAL_H_ */
