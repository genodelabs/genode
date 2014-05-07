/*
 * \brief  libc string manipulation
 * \author Sebastian Sumpf
 * \date   2009-10-26
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <string.h>
#include <util/string.h>

extern "C"
void bzero(void *s, size_t n)
{
	Genode::memset(s, 0, n);
}

extern "C"
int strcmp(const char *s1, const char *s2)
{
	return Genode::strcmp(s1, s2);
}

extern "C"
int strncmp(const char *s1, const char *s2, size_t n)
{
	return Genode::strcmp(s1, s2, n);
}
extern "C"
size_t strlen(const char *s)
{
	return Genode::strlen(s);
}

extern "C"
char *strncpy(char *dst, const char *src, size_t n)
{
	return Genode::strncpy(dst, src, n);
}

extern "C"
void *memcpy(void *dest, const void *src, size_t n)
{
	return Genode::memcpy(dest, src, n);
}

extern "C"
void *memset(void *s, int c, size_t n)
{
	void *r = Genode::memset(s, c, n);
	return r;
}
