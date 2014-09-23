/*
 * \brief  VirtualBox utilities
 * \author Christian Helmuth
 * \date   2013-08-28
 */

/*
 * Copyright (C) 2013-2014 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _UTIL_H_
#define _UTIL_H_

/* VirtualBox includes */
#include <iprt/types.h>


/**
 * 64bit-aware cast of pointer to RTRCPTR (uint32_t)
 */
template <typename T>
RTRCPTR to_rtrcptr(T* ptr)
{
	unsigned long long u64 = reinterpret_cast<unsigned long long>(ptr);
	RTRCPTR rtrcptr = u64 & 0xFFFFFFFFULL;

	AssertMsg((u64 == rtrcptr) || (u64 >> 32) == 0xFFFFFFFFULL,
	          ("pointer transformation - %llx != %x", u64, rtrcptr));

	return rtrcptr;
}

#endif /* _UTIL_H_ */
