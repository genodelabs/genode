/*
 * \brief  TrustZone specific definitions for the Versatile Express board
 * \author Stefan Kalkowski
 * \date   2013-11-15
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PLATFORM__VEA9X4__DRIVERS__TRUSTZONE_H_
#define _INCLUDE__PLATFORM__VEA9X4__DRIVERS__TRUSTZONE_H_

/* Genode includes */
#include <drivers/board_base.h>

namespace Trustzone
{
	enum {
		SECURE_RAM_BASE    = Genode::Board_base::RAM_3_BASE,
		SECURE_RAM_SIZE    = Genode::Board_base::RAM_3_SIZE,
		NONSECURE_RAM_BASE = 0x80000000,
		NONSECURE_RAM_SIZE = 0x20000000,
	};
}

#endif /* _INCLUDE__PLATFORM__VEA9X4__DRIVERS__TRUSTZONE_H_ */

