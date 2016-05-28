/*
 * \brief  Board definitions for the i.MX53 starter board
 * \author Stefan Kalkowski
 * \date   2012-10-24
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SPEC__USB_ARMORY__DRIVERS__BOARD_BASE_H_
#define _INCLUDE__SPEC__USB_ARMORY__DRIVERS__BOARD_BASE_H_

/* Genode includes */
#include <spec/imx53/drivers/board_base_support.h>

namespace Genode { struct Board_base; }


/**
 * i.MX53 starter board
 */
struct Genode::Board_base : Imx53::Board_base
{
	enum {
		/*
		 * These two regions are physically one RAM region but we split it
		 * to keep the enum names compliant with other i.MX53 boards. This
		 * way, more files can be shared between the platforms.
		 */
		RAM0_BASE = 0x70000000,
		RAM0_SIZE = 0x10000000,
		RAM1_BASE = 0x80000000,
		RAM1_SIZE = 0x10000000,
	};
};

#endif /* _INCLUDE__SPEC__USB_ARMORY__DRIVERS__BOARD_BASE_H_ */
