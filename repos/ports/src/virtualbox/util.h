/*
 * \brief  VirtualBox utilities
 * \author Christian Helmuth
 * \date   2013-08-28
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
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
	return (RTRCPTR)((long)ptr & 0xffffffff);
}

/**
 * 64bit-aware cast of RTRCPTR (uint32_t) to pointer
 */
template <typename T>
T* from_rtrcptr(RTRCPTR rcptr)
{
	return (T*)(rcptr | 0L);
}

#endif /* _UTIL_H_ */
