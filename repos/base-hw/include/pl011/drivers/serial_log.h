/*
 * \brief  Serial output driver specific for the ARM PL011
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PL011__DRIVERS__SERIAL_LOG_H_
#define _INCLUDE__PL011__DRIVERS__SERIAL_LOG_H_

/* Genode includes */
#include <board.h>
#include <drivers/uart/pl011_base.h>

namespace Genode
{
	/**
	 * Serial output driver specific for the ARM PL011
	 */
	class Serial_log : public Pl011_base
	{
		public:

			/**
			 * Constructor
			 *
			 * \param baud_rate  targeted transfer baud-rate
			 */
			Serial_log(unsigned const baud_rate) :
				Pl011_base(Board::PL011_0_MMIO_BASE,
				           Board::PL011_0_CLOCK, baud_rate)
			{ }
	};
}

#endif /* _INCLUDE__PL011__DRIVERS__SERIAL_LOG_H_ */

