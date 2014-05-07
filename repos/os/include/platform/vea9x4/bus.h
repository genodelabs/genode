/*
 * \brief  Bus definitions for the Versatile Express platform
 * \author Stefan Kalkowski
 * \date   2011-11-08
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PLATFORM__VEA9X4__BUS_H_
#define _INCLUDE__PLATFORM__VEA9X4__BUS_H_

/**
 * Static Memory Bus (SMB) addresses
 */
enum {
	SMB_CS0 = 0x40000000,
	SMB_CS1 = 0x44000000,
	SMB_CS2 = 0x48000000,
	SMB_CS3 = 0x4c000000,
	SMB_CS4 = 0x50000000,
	SMB_CS5 = 0x54000000,
	SMB_CS6 = 0x58000000,
	SMB_CS7 = 0x10000000,
};

#endif /* _INCLUDE__PLATFORM__VEA9X4__BUS_H_ */
