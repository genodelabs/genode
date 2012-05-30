/**
 * \brief  Motherboard driver specific for the Realview PBXA9
 * \author Martin Stein
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PLATFORM__PBXA9__DRIVERS__BOARD_H_
#define _INCLUDE__PLATFORM__PBXA9__DRIVERS__BOARD_H_

/* Genode includes */
#include <drivers/board/pbxa9.h>
#include <base/native_types.h>

namespace Genode
{
	/**
	 * Provide specific board driver
	 */
	class Board : public Pbxa9
	{
		public:

			enum {
				LOG_PL011_MMIO_BASE = PL011_0_MMIO_BASE,
				LOG_PL011_CLOCK     = PL011_0_CLOCK,
			};
	};
}

#endif /* _INCLUDE__PLATFORM__PBXA9__DRIVERS__BOARD_H_ */

