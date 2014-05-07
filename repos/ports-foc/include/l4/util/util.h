/*
 * \brief  L4Re functions needed by L4Linux
 * \author Stefan Kalkowski
 * \date   2011-03-17
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _L4__UTIL__UTIL_H_
#define _L4__UTIL__UTIL_H_

#include <l4/sys/types.h>
#include <l4/sys/compiler.h>
#include <l4/sys/ipc.h>

L4_CV void l4_sleep(int ms);

L4_CV void l4_sleep_forever(void) __attribute__((noreturn));


static inline void
l4_touch_ro(const void*addr, unsigned size)
{
	volatile const char *bptr, *eptr;

	bptr = (const char*)(((unsigned)addr) & L4_PAGEMASK);
	eptr = (const char*)(((unsigned)addr+size-1) & L4_PAGEMASK);
	for(;bptr<=eptr;bptr+=L4_PAGESIZE) {
		(void)(*bptr);
	}
}


static inline void
l4_touch_rw(const void*addr, unsigned size)
{
	volatile char *bptr;
	volatile const char *eptr;

	bptr = (char*)(((unsigned)addr) & L4_PAGEMASK);
	eptr = (const char*)(((unsigned)addr+size-1) & L4_PAGEMASK);
	for(;bptr<=eptr;bptr+=L4_PAGESIZE) {
		char x = *bptr;
		*bptr = x;
	}
}

#endif /* _L4__UTIL__UTIL_H_ */
