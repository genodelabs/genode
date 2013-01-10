/*
 * \brief   String function wrappers for Genode
 * \date    2008-07-24
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _GENODE_STRING_H_
#define _GENODE_STRING_H_

#include <util/string.h>

inline Genode::size_t strlen(const char *s)
{ return Genode::strlen(s); }

inline void *memset(void *s, int c, Genode::size_t n)
{ return Genode::memset(s, c, n); }

inline void *memcpy(void *dest, const void *src, Genode::size_t n) {
return Genode::memcpy(dest, src, n); }

#endif
