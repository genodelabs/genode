/*
 * \brief  Board driver for core on Zynq
 * \author Johannes Schlatow
 * \date   2014-12-15
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BOARD_H_
#define _BOARD_H_

/* core includes */
#include <spec/zynq/board_support.h>

namespace Genode
{
	class Board : public Zynq::Board
	{
		public:
			enum {
				UART_MMIO_BASE = UART_0_MMIO_BASE
			};
	};
}

#endif /* _BOARD_H_ */
