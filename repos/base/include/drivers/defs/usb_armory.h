/*
 * \brief  MMIO and IRQ definitions for the USB Armory
 * \author Stefan Kalkowski
 * \date   2012-10-24
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DRIVERS__DEFS__USB_ARMORY_H_
#define _INCLUDE__DRIVERS__DEFS__USB_ARMORY_H_

/* Genode includes */
#include <drivers/defs/imx53.h>

namespace Usb_armory {

	using namespace Imx53;

	enum {
		RAM_BASE = RAM_BANK_0_BASE,
		RAM_SIZE = 0x20000000,
	};
};

#endif /* _INCLUDE__DRIVERS__DEFS__USB_ARMORY_H_ */
