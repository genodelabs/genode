/*
 * \brief  Serial output driver for core
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SPEC__PL011__SERIAL_H_
#define _CORE__INCLUDE__SPEC__PL011__SERIAL_H_

/* core includes */
#include <board.h>
#include <platform.h>

/* Genode includes */
#include <drivers/uart_base.h>

namespace Genode { class Serial; }


class Genode::Serial : public Pl011_base
{
	public:

		/**
		 * Constructor
		 *
		 * \param baud_rate  targeted transfer baud-rate
		 */
		Serial(unsigned const baud_rate)
		:
			Pl011_base(Platform::mmio_to_virt(Board::PL011_0_MMIO_BASE),
			           Board::PL011_0_CLOCK, baud_rate)
		{ }
};

#endif /* _CORE__INCLUDE__SPEC__PL011__SERIAL_H_ */
