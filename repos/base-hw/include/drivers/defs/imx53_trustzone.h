/*
 * \brief  TrustZone specific definitions for the i.MX53 board
 * \author Stefan Kalkowski
 * \date   2013-11-15
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__IMX53__DRIVERS__TRUSTZONE_H_
#define _INCLUDE__SPEC__IMX53__DRIVERS__TRUSTZONE_H_

/* Genode includes */
#include <drivers/defs/imx53.h>

namespace Trustzone
{
	enum {
		/**
		 * Currently, we limit secure RAM to 256 MB only,
		 * because the memory protection feature of the M4IF
		 * on this platform allows to protect a max. region of
		 * 256MB per RAM bank only.
		 */
		SECURE_RAM_BASE    = Imx53::RAM_BANK_0_BASE,
		SECURE_RAM_SIZE    = 256 * 1024 * 1024,
		NONSECURE_RAM_BASE = Imx53::RAM_BANK_0_BASE + SECURE_RAM_SIZE,
		NONSECURE_RAM_SIZE = 256 * 1024 * 1024,
	};
}

#endif /* _INCLUDE__SPEC__IMX53__DRIVERS__TRUSTZONE_H_ */
