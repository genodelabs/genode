/*
 * \brief  Serial output driver for core
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SERIAL_H_
#define _SERIAL_H_

/* core includes */
#include <board.h>

/* Genode includes */
#include <drivers/uart/tl16c750_base.h>

namespace Genode
{
	/**
	 * Serial output driver for core
	 */
	class Serial : public Tl16c750_base
	{
		public:

			/**
			 * Constructor
			 *
			 * \param baud_rate  targeted transfer baud-rate
			 */
			Serial(unsigned const baud_rate)
			:
				Tl16c750_base(Board::TL16C750_3_MMIO_BASE,
				              Board::TL16C750_CLOCK, baud_rate)
			{ }
	};
}

#endif /* _SERIAL_H_ */
