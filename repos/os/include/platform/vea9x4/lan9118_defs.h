/*
 * \brief  LAN 9118 NIC controller definitions for the RealView platform
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2010-03-23
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PLATFORM__VEA9X4__LAN9118_DEFS_H_
#define _INCLUDE__PLATFORM__VEA9X4__LAN9118_DEFS_H_

#include <platform/vea9x4/bus.h>
#include <drivers/board_base.h>

enum {

	/**
	 * Base address of MMIO resource
	 */
	LAN9118_PHYS = SMB_CS3 + 0x02000000,

	/**
	 * Size of MMIO resource
	 *
	 * On the RealView platform, the device spans actually a much larger
	 * resource. However, only the first page is used.
	 */
	LAN9118_SIZE = 0x1000,

	/**
	 * Interrupt line
	 */
	LAN9118_IRQ = Genode::Board_base::LAN9118_IRQ,
};

#endif /* _INCLUDE__PLATFORM__VEA9X4__LAN9118_DEFS_H_ */
