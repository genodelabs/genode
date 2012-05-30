/**
 * \brief  Motherboard driver specific for the PandaBoard A2
 * \author Martin Stein
 * \date   2012-03-08
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PLATFORM__PANDA_A2__DRIVERS__BOARD_H_
#define _INCLUDE__PLATFORM__PANDA_A2__DRIVERS__BOARD_H_

/* Genode includes */
#include <drivers/board/panda_a2.h>

namespace Genode
{
	/**
	 * Provide specific board driver
	 */
	class Board : public Panda_a2
	{
		public:

			enum {
				LOG_TL16C750_MMIO_BASE = TL16C750_3_MMIO_BASE,
				LOG_TL16C750_CLOCK     = TL16C750_3_CLOCK,
			};
	};
}

#endif /* _INCLUDE__PLATFORM__PANDA_A2__DRIVERS__BOARD_H_ */

