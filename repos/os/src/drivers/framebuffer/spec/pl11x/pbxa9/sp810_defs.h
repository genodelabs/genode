/*
 * \brief  SP810 System Controller.
 * \author Stefan Kalkowski
 * \date   2011-11-14
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__PBXA9__SP810_DEFS_H_
#define _INCLUDE__SPEC__PBXA9__SP810_DEFS_H_

enum {

	SP810_PHYS        = 0x10001000,
	SP810_SIZE        = 0x1000,

	SP810_REG_ID      = 0x0,
	SP810_REG_OSCCLCD = 0x1c,
	SP810_REG_LOCK    = 0x20,
};

#endif /* _INCLUDE__SPEC__PBXA9__SP810_DEFS_H_ */
