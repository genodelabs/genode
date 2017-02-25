/*
 * \brief  Genode backend for GDB server (x86_64-specific code)
 * \author Christian Prochaska
 * \date   2014-01-30
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef AMD64_H
#define AMD64_H

/* indices from 'regs_amd64' array in amd64.c */
enum reg_index {
	RAX      =  0,
	RBX      =  1,
	RCX      =  2,
	RDX      =  3,
	RSI      =  4,
	RDI      =  5,
	RBP      =  6,
	RSP      =  7,
	R8       =  8,
	R9       =  9,
	R10      = 10,
	R11      = 11,
	R12      = 12,
	R13      = 13,
	R14      = 14,
	R15      = 15,
	RIP      = 16,
	EFLAGS   = 17,
	CS       = 18,
	SS       = 19,
	DS       = 20,
	ES       = 21,
	FS       = 22,
	GS       = 23,
	ST0      = 24,
	ST1      = 25,
	ST2      = 26,
	ST3      = 27,
	ST4      = 28,
	ST5      = 29,
	ST6      = 30,
	ST7      = 31,
	FCTRL    = 32,
	FSTAT    = 33,
	FTAG     = 34,
	FISEG    = 35,
	FIOFF    = 36,
	FOSEG    = 37,
	FOOFF    = 38,
	FOP      = 39,
	XMM0     = 40,
	XMM1     = 41,
	XMM2     = 42,
	XMM3     = 43,
	XMM4     = 44,
	XMM5     = 45,
	XMM6     = 46,
	XMM7     = 47,
	XMM8     = 48,
	XMM9     = 49,
	XMM10    = 50,
	XMM11    = 51,
	XMM12    = 52,
	XMM13    = 53,
	XMM14    = 54,
	XMM15    = 55,
	MXCSR    = 56,
};

#endif /* AMD64_H */
