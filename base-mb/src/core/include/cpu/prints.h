/*
 * \brief  Saver print methods than the luxury dynamic-number/type-of-arguments one's
 * \author Martin Stein
 * \date   2010-09-16
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__XMB__PRINTS_H_
#define _INCLUDE__XMB__PRINTS_H_


#include <cpu/config.h>


enum { UART_OUT_REGISTER=0x84000004 };


inline static void _prints_chr1(volatile char chr1)
{
	unsigned volatile* uart = (volatile unsigned*)UART_OUT_REGISTER;
	*uart = chr1;
}


inline static void _prints_hex2(volatile char hex2)
{
	volatile char hex1 = ((hex2 >> 4) & 0xf);
	if (hex1 > 9) hex1 += 39;
	hex1 += 48;
	_prints_chr1((volatile char)hex1);

	hex1 = hex2 & 0xf;
	if (hex1 > 9) hex1 += 39;
	hex1 += 48;
	_prints_chr1((volatile char)hex1);
}


inline static void _prints_hex8(unsigned volatile hex8)
{
	_prints_hex2((volatile char)(hex8 >> 24));
	_prints_hex2((volatile char)(hex8 >> 16));
	_prints_hex2((volatile char)(hex8 >> 8));
	_prints_hex2((volatile char)(hex8 >> 0));
}


inline static void _prints_hex8l(unsigned volatile hex8)
{
	_prints_hex8(hex8);
	_prints_chr1('\n');
}


inline static void _prints_str0(const char* volatile str0)
{
	while (*str0) _prints_chr1(*str0++);
}


#endif /* _INCLUDE__XMB__PRINTS_H_ */
