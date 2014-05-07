/*
 * \brief  libc I/O functions
 * \author Sebastian Sumpf
 * \date   2009-10-26
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <base/snprintf.h>
#include <stdio.h>

extern "C" int printf(char const *format, ...)
{
	va_list args;

	va_start(args, format);
	Genode::vprintf(format, args);
	va_end(args);
		
	return 0;
}

extern "C" inline
int vprintf(const char *format, va_list ap) 
{
	Genode::vprintf(format, ap);
	return 0;
}

extern "C" 
int vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
	Genode::String_console sc(str, size);
	sc.vprintf(format, ap);
	return sc.len();
}

extern "C"
int vfprintf(int *stream, const char *format, va_list ap)
{
	(void)stream;
	return vprintf(format, ap);
}

extern "C"
int putchar(int c) {
	printf("%c", c);
	return c;
}

extern "C"
int putc(int c, int *stream)
{
	(void)stream;
	printf("%c", c);
	return c;
}
