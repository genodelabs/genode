/*
 * \brief  Genode backend for GDB server (x86_32-specific code)
 * \author Christian Prochaska
 * \date   2011-07-07
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef I386_LINUX_H
#define I386_LINUX_H

/* indices from 'regs_i386' array in i386.c */
enum reg_index {
	EAX      =  0,
	ECX      =  1,
	EDX      =  2,
	EBX      =  3,
	UESP     =  4,
	EBP      =  5,
	ESI      =  6,
	EDI      =  7,
	EIP      =  8,
	EFL      =  9,
	CS       = 10,
	SS       = 11,
	DS       = 12,
	ES       = 13,
	FS       = 14,
	GS       = 15,
};

#endif /* I386_LINUX_H */
